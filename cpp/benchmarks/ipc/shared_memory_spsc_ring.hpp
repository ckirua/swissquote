#pragma once

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

namespace swissquote::benchmark::ipc {

class SharedMemorySpscRing {
public:
    static constexpr std::uint32_t kCapacity = 2048;
    static constexpr std::uint32_t kMaxPayload = 65536;
    static constexpr std::uint32_t kVersion = 1;

    struct Config {
        std::uint32_t capacity{kCapacity};
        std::uint32_t max_payload{kMaxPayload};
    };

    static SharedMemorySpscRing create(std::string name) {
        return create(std::move(name), Config{kCapacity, kMaxPayload});
    }
    static SharedMemorySpscRing create(std::string name, Config config) {
        validate_name(name);
        validate_config(config);
        const int fd = ::shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0) throw_errno("shm_open create");
        try {
            const auto bytes = mapping_size(config);
            if (::ftruncate(fd, static_cast<off_t>(bytes)) != 0) throw_errno("ftruncate");
            void* mapping = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mapping == MAP_FAILED) throw_errno("mmap");
            auto* header = new (mapping) Header{};
            header->magic = kMagic;
            header->version = kVersion;
            header->capacity = config.capacity;
            header->max_payload = config.max_payload;
            header->mapping_bytes = bytes;
            std::atomic_thread_fence(std::memory_order_release);
            return SharedMemorySpscRing(std::move(name), fd, header, bytes, true);
        } catch (...) {
            ::close(fd);
            ::shm_unlink(name.c_str());
            throw;
        }
    }

    static SharedMemorySpscRing attach(std::string name) {
        return attach(std::move(name), Config{kCapacity, kMaxPayload});
    }
    static SharedMemorySpscRing attach(std::string name, Config config) {
        validate_name(name);
        validate_config(config);
        const int fd = ::shm_open(name.c_str(), O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0) throw_errno("shm_open attach");
        try {
            struct stat status{};
            if (::fstat(fd, &status) != 0) throw_errno("fstat");
            const auto expected = mapping_size(config);
            if (static_cast<std::uint64_t>(status.st_size) != expected) {
                throw std::runtime_error("shared ring mapping size mismatch");
            }
            void* mapping = ::mmap(nullptr, expected, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mapping == MAP_FAILED) throw_errno("mmap");
            auto* header = static_cast<Header*>(mapping);
            if (header->magic != kMagic || header->version != kVersion
                || header->capacity != config.capacity || header->max_payload != config.max_payload
                || header->mapping_bytes != expected) {
                ::munmap(mapping, expected);
                throw std::runtime_error("shared ring ABI/config mismatch");
            }
            return SharedMemorySpscRing(std::move(name), fd, header, expected, false);
        } catch (...) {
            ::close(fd);
            throw;
        }
    }

    SharedMemorySpscRing(const SharedMemorySpscRing&) = delete;
    SharedMemorySpscRing& operator=(const SharedMemorySpscRing&) = delete;
    SharedMemorySpscRing(SharedMemorySpscRing&& other) noexcept { swap(other); }
    SharedMemorySpscRing& operator=(SharedMemorySpscRing&& other) noexcept {
        if (this != &other) { close(); swap(other); }
        return *this;
    }
    ~SharedMemorySpscRing() { close(); }

    [[nodiscard]] std::byte* claim() noexcept {
        if (claimed_ || header_ == nullptr) return nullptr;
        const auto head = header_->producer.load(std::memory_order_relaxed);
        const auto tail = header_->consumer.load(std::memory_order_acquire);
        if (head - tail >= header_->capacity) return nullptr;
        claimed_ = true;
        claimed_index_ = head;
        return payload(head);
    }

    bool publish(std::size_t length) noexcept {
        if (!claimed_ || length > header_->max_payload) return false;
        Slot* slot = slot_at(claimed_index_);
        slot->length = static_cast<std::uint32_t>(length);
        const bool was_empty = claimed_index_ == header_->consumer.load(std::memory_order_acquire);
        header_->producer.store(claimed_index_ + 1, std::memory_order_release);
        claimed_ = false;
        if (was_empty) {
            header_->notify.fetch_add(1, std::memory_order_release);
            futex_wake(&header_->notify);
        }
        return true;
    }

    bool consume_into(void* output, std::size_t capacity, std::size_t& length) noexcept {
        if (header_ == nullptr) return false;
        const auto tail = header_->consumer.load(std::memory_order_relaxed);
        const auto head = header_->producer.load(std::memory_order_acquire);
        if (tail == head) return false;
        Slot* slot = slot_at(tail);
        if (slot->length > capacity) return false;
        std::memcpy(output, slot->bytes, slot->length);
        length = slot->length;
        header_->consumer.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool wait_for_data(std::uint32_t observed, std::uint32_t timeout_ns = 1000000) noexcept {
        timespec timeout{0, static_cast<long>(timeout_ns)};
        const long result = ::syscall(SYS_futex, &header_->notify, FUTEX_WAIT, observed, &timeout, nullptr, 0);
        return result == 0 || errno == EAGAIN || errno == EINTR || errno == ETIMEDOUT;
    }
    [[nodiscard]] std::uint32_t notification_sequence() const noexcept {
        return header_->notify.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool empty() const noexcept {
        return header_->producer.load(std::memory_order_acquire)
            == header_->consumer.load(std::memory_order_acquire);
    }
    static bool unlink(std::string_view name) noexcept {
        const std::string owned{name};
        return ::shm_unlink(owned.c_str()) == 0 || errno == ENOENT;
    }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    static constexpr std::uint64_t kMagic = 0x53515152494e4731ULL;
    struct alignas(64) Header {
        std::uint64_t magic{};
        std::uint32_t version{};
        std::uint32_t capacity{};
        std::uint32_t max_payload{};
        std::uint32_t reserved{};
        std::uint64_t mapping_bytes{};
        alignas(64) std::atomic<std::uint64_t> producer{0};
        alignas(64) std::atomic<std::uint64_t> consumer{0};
        alignas(64) std::atomic<std::uint32_t> notify{0};
    };
    struct Slot { std::uint32_t length; std::byte bytes[1]; };

    SharedMemorySpscRing(std::string name, int fd, Header* header, std::size_t bytes, bool creator)
        : name_(std::move(name)), fd_(fd), header_(header), bytes_(bytes), creator_(creator) {}
    static void validate_name(const std::string& name) {
        if (name.size() < 2 || name.front() != '/') throw std::invalid_argument("shm name must start with /");
    }
    static void validate_config(Config config) {
        if (config.capacity != kCapacity || config.max_payload != kMaxPayload) {
            throw std::invalid_argument("benchmark ring requires 2048 slots and 65536-byte payloads");
        }
    }
    static std::size_t mapping_size(Config config) {
        return sizeof(Header) + static_cast<std::size_t>(config.capacity) * (sizeof(std::uint32_t) + config.max_payload);
    }
    static void throw_errno(const char* operation) { throw std::system_error(errno, std::generic_category(), operation); }
    static void futex_wake(std::atomic<std::uint32_t>* address) noexcept {
        ::syscall(SYS_futex, address, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }
    Slot* slot_at(std::uint64_t index) const noexcept {
        auto* base = reinterpret_cast<std::byte*>(header_) + sizeof(Header);
        constexpr std::size_t stride = sizeof(std::uint32_t) + kMaxPayload;
        return reinterpret_cast<Slot*>(base + (index % kCapacity) * stride);
    }
    std::byte* payload(std::uint64_t index) const noexcept { return slot_at(index)->bytes; }
    void close() noexcept {
        if (header_ != nullptr) {
            ::munmap(header_, bytes_);
            header_ = nullptr;
        }
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
        if (creator_) { ::shm_unlink(name_.c_str()); creator_ = false; }
    }
    void swap(SharedMemorySpscRing& other) noexcept {
        using std::swap;
        swap(name_, other.name_); swap(fd_, other.fd_); swap(header_, other.header_);
        swap(bytes_, other.bytes_); swap(creator_, other.creator_); swap(claimed_, other.claimed_);
        swap(claimed_index_, other.claimed_index_);
    }
    std::string name_;
    int fd_{-1};
    Header* header_{};
    std::size_t bytes_{};
    bool creator_{};
    bool claimed_{};
    std::uint64_t claimed_index_{};
};

} // namespace swissquote::benchmark::ipc
