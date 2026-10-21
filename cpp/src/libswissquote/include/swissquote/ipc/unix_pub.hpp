#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <span>
#include <string>
#include <thread>

#include "swissquote/ipc/spsc_ring.hpp"

namespace swissquote::ipc {

struct PubStats {
    std::uint64_t sent{};
    std::uint64_t dropped_queue{};
    std::uint64_t dropped_no_client{};
    std::uint64_t dropped_clients{};
    std::uint64_t send_errors{};
    std::size_t queue_size{};
    std::size_t client_count{};
};

class ThreadedUnixSeqpacketPub {
public:
    static constexpr std::size_t kRingCapacity = 2048;
    static constexpr std::size_t kMaxPayload = 65536;
    static constexpr std::size_t kMaxClients = 64;
    static constexpr double kStartupTimeoutSec = 5.0;

    explicit ThreadedUnixSeqpacketPub(std::string socket_path);
    ~ThreadedUnixSeqpacketPub();

    ThreadedUnixSeqpacketPub(const ThreadedUnixSeqpacketPub&) = delete;
    ThreadedUnixSeqpacketPub& operator=(const ThreadedUnixSeqpacketPub&) = delete;

    void start(double timeout_sec = kStartupTimeoutSec);
    void close();
    void publish_nowait(std::span<const std::byte> payload);

    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] PubStats stats() const;

private:
    void worker();
    void wake_worker() noexcept;

    std::string socket_path_;
    SpscBytesRing ring_{kRingCapacity, kMaxPayload};
    std::thread thread_;
    std::mutex publish_mu_;
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> alive_{false};
    std::atomic<int> wake_fd_{-1};
    std::atomic<std::uint64_t> sent_{0};
    std::atomic<std::uint64_t> dropped_no_client_{0};
    std::atomic<std::uint64_t> dropped_clients_{0};
    std::atomic<std::uint64_t> send_errors_{0};
    std::atomic<std::size_t> client_count_{0};
    std::exception_ptr worker_error_;
};

} // namespace swissquote::ipc
