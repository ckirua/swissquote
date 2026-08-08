#include "swissquote/zmq/spsc_ring.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace swissquote::zmq {
namespace {

inline void write_u32_le(char* dst, std::uint32_t value) noexcept {
    dst[0] = static_cast<char>(value & 0xFF);
    dst[1] = static_cast<char>((value >> 8) & 0xFF);
    dst[2] = static_cast<char>((value >> 16) & 0xFF);
    dst[3] = static_cast<char>((value >> 24) & 0xFF);
}

inline std::uint32_t read_u32_le(const char* src) noexcept {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(src[0]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(src[1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(src[2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(src[3])) << 24);
}

} // namespace

SpscBytesRing::SpscBytesRing(std::size_t capacity, std::size_t max_payload)
    : capacity_(capacity), mask_(capacity - 1), max_payload_(max_payload), slot_bytes_(4 + max_payload) {
    if (!is_pow2(capacity)) {
        throw std::invalid_argument("capacity must be a power of 2");
    }
    if (max_payload == 0) {
        throw std::invalid_argument("max_payload must be > 0");
    }
    const std::size_t buf_size = slot_bytes_ * capacity_;
    buf_ = new char[buf_size];
}

SpscBytesRing::~SpscBytesRing() {
    delete[] buf_;
}

bool SpscBytesRing::is_pow2(std::size_t n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

char* SpscBytesRing::slot_ptr(std::size_t index) noexcept {
    return buf_ + ((index & mask_) * slot_bytes_);
}

bool SpscBytesRing::is_full(std::size_t head, std::size_t tail) const noexcept {
    return (tail - head) == capacity_;
}

std::size_t SpscBytesRing::size_approx() const noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    return tail - head;
}

bool SpscBytesRing::try_push(std::span<const std::byte> payload) {
    return try_push_bytes(reinterpret_cast<const char*>(payload.data()), payload.size());
}

bool SpscBytesRing::try_push_bytes(const char* data, std::size_t length) {
    if (length > max_payload_) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (is_full(head, tail)) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    char* slot = slot_ptr(tail);
    write_u32_le(slot, static_cast<std::uint32_t>(length));
    if (length > 0) {
        std::memcpy(slot + 4, data, length);
    }
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}

std::optional<std::vector<std::byte>> SpscBytesRing::try_pop() {
    const std::byte* data = nullptr;
    std::size_t length = 0;
    if (!try_peek_view(&data, &length)) {
        return std::nullopt;
    }
    std::vector<std::byte> out(length);
    if (length > 0) {
        std::memcpy(out.data(), data, length);
    }
    try_commit_pop();
    return out;
}

bool SpscBytesRing::try_peek_view(const std::byte** data, std::size_t* length) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) {
        return false;
    }
    char* slot = slot_ptr(head);
    *length = read_u32_le(slot);
    *data = reinterpret_cast<const std::byte*>(slot + 4);
    return true;
}

bool SpscBytesRing::try_commit_pop() noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) {
        return false;
    }
    head_.store(head + 1, std::memory_order_release);
    return true;
}

} // namespace swissquote::zmq
