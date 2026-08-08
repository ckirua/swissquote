#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace swissquote::zmq {

class SpscBytesRing {
public:
    SpscBytesRing(std::size_t capacity, std::size_t max_payload);
    ~SpscBytesRing();

    SpscBytesRing(const SpscBytesRing&) = delete;
    SpscBytesRing& operator=(const SpscBytesRing&) = delete;

    [[nodiscard]] std::size_t size_approx() const noexcept;
    [[nodiscard]] std::uint64_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }

    bool try_push(std::span<const std::byte> payload);
    bool try_push_bytes(const char* data, std::size_t length);
    [[nodiscard]] std::optional<std::vector<std::byte>> try_pop();

    bool try_peek_view(const std::byte** data, std::size_t* length) noexcept;
    bool try_commit_pop() noexcept;

private:
    [[nodiscard]] char* slot_ptr(std::size_t index) noexcept;
    [[nodiscard]] static bool is_pow2(std::size_t n) noexcept;
    [[nodiscard]] bool is_full(std::size_t head, std::size_t tail) const noexcept;

    std::size_t capacity_{};
    std::size_t mask_{};
    std::size_t max_payload_{};
    std::size_t slot_bytes_{};
    char* buf_{nullptr};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::atomic<std::uint64_t> dropped_{0};
};

} // namespace swissquote::zmq
