#pragma once

#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <thread>

#include "swissquote/zmq/spsc_ring.hpp"

namespace swissquote::zmq {

struct PubStats {
    std::uint64_t sent{};
    std::uint64_t dropped_queue{};
    std::uint64_t dropped_zmq{};
    std::uint64_t send_errors{};
    std::size_t queue_size{};
};

class ThreadedZmqPub {
public:
    static constexpr int kRingCapacity = 8192;
    static constexpr std::size_t kMaxPayload = 65536;
    static constexpr int kSocketHwm = 8192;
    static constexpr double kStartupTimeoutSec = 5.0;

    explicit ThreadedZmqPub(std::string endpoint);
    ~ThreadedZmqPub();

    ThreadedZmqPub(const ThreadedZmqPub&) = delete;
    ThreadedZmqPub& operator=(const ThreadedZmqPub&) = delete;

    void start(double timeout_sec = kStartupTimeoutSec);
    void close();
    void publish_nowait(std::span<const std::byte> payload);

    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] PubStats stats() const;

private:
    void worker();
    bool process_view(void* socket, const std::byte* data, std::size_t len);
    bool drain_available(void* socket);

    std::string endpoint_;
    SpscBytesRing ring_{kRingCapacity, kMaxPayload};
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> alive_{false};
    std::atomic<std::uint64_t> sent_{0};
    std::atomic<std::uint64_t> dropped_zmq_{0};
    std::atomic<std::uint64_t> send_errors_{0};
    std::exception_ptr worker_error_;
};

} // namespace swissquote::zmq
