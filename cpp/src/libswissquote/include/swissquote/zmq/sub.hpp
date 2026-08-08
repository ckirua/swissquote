#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <span>
#include <thread>
#include <vector>

#include "swissquote/zmq/spsc_ring.hpp"

namespace swissquote::zmq {

struct SubStats {
    std::uint64_t received{};
    std::uint64_t dropped_queue{};
    std::uint64_t recv_errors{};
    std::size_t queue_size{};
};

class ThreadedZmqSub {
public:
    static constexpr int kRingCapacity = 8192;
    static constexpr std::size_t kMaxPayload = 65536;
    static constexpr int kSocketHwm = 8192;
    static constexpr double kStartupTimeoutSec = 5.0;

    explicit ThreadedZmqSub(std::string endpoint);
    ~ThreadedZmqSub();

    ThreadedZmqSub(const ThreadedZmqSub&) = delete;
    ThreadedZmqSub& operator=(const ThreadedZmqSub&) = delete;

    void start(double timeout_sec = kStartupTimeoutSec);
    void close();

    [[nodiscard]] std::optional<std::vector<std::byte>> try_pop_frame();
    [[nodiscard]] std::optional<std::vector<std::byte>> pop_wait_frame(double timeout_sec);

    template <typename Consumer>
    bool consume_wait_frame(double timeout_sec, Consumer&& consumer) {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(timeout_sec));
        while (true) {
            const std::byte* data = nullptr;
            std::size_t length = 0;
            if (ring_.try_peek_view(&data, &length)) {
                consumer(std::span<const std::byte>(data, length));
                ring_.try_commit_pop();
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::unique_lock lock(wake_mu_);
            wake_cv_.wait_for(lock, std::chrono::milliseconds(1));
        }
    }

    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] SubStats stats() const;

private:
    void worker();
    bool recv_available(void* socket);

    std::string endpoint_;
    SpscBytesRing ring_{kRingCapacity, kMaxPayload};
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> alive_{false};
    std::atomic<std::uint64_t> received_{0};
    std::atomic<std::uint64_t> recv_errors_{0};
    std::exception_ptr worker_error_;

    bool pending_push_{false};
    std::vector<std::byte> pending_frame_;
    std::mutex wake_mu_;
    std::condition_variable wake_cv_;
};

} // namespace swissquote::zmq
