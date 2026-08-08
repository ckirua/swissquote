#include "swissquote/zmq/pub.hpp"

#include "swissquote/zmq/zmq_io.hpp"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>

#include <zmq.h>

namespace swissquote::zmq {
namespace {

constexpr int kZmqTcpNodelay = 34;

void set_int_option(void* socket, int option, int value) {
    if (zmq_setsockopt(socket, option, &value, sizeof(value)) != 0) {
        throw std::runtime_error("zmq_setsockopt failed");
    }
}

} // namespace

ThreadedZmqPub::ThreadedZmqPub(std::string endpoint) : endpoint_(std::move(endpoint)) {}

ThreadedZmqPub::~ThreadedZmqPub() {
    close();
}

bool ThreadedZmqPub::is_healthy() const {
    return alive_.load(std::memory_order_acquire) && started_.load(std::memory_order_relaxed);
}

PubStats ThreadedZmqPub::stats() const {
    return PubStats{
        .sent = sent_.load(std::memory_order_relaxed),
        .dropped_queue = ring_.dropped(),
        .dropped_zmq = dropped_zmq_.load(std::memory_order_relaxed),
        .send_errors = send_errors_.load(std::memory_order_relaxed),
        .queue_size = ring_.size_approx(),
    };
}

bool ThreadedZmqPub::process_view(void* socket, const std::byte* data, std::size_t len) {
    const int rc = io::try_send_bytes(socket, reinterpret_cast<const char*>(data), len);
    if (rc == 0) {
        sent_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    if (rc == 1) {
        return false;
    }
    send_errors_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool ThreadedZmqPub::drain_available(void* socket) {
    const std::byte* data = nullptr;
    std::size_t len = 0;
    bool got = false;
    bool keep_draining = true;

    while (keep_draining && running_.load(std::memory_order_relaxed)) {
        if (!ring_.try_peek_view(&data, &len)) {
            break;
        }
        got = true;
        keep_draining = process_view(socket, data, len);
        if (keep_draining) {
            ring_.try_commit_pop();
        } else {
            break;
        }
    }
    return got;
}

void ThreadedZmqPub::worker() {
    void* ctx = zmq_ctx_new();
    void* socket = nullptr;
    try {
        if (ctx == nullptr) {
            throw std::runtime_error("zmq_ctx_new failed");
        }
        socket = zmq_socket(ctx, ZMQ_PUB);
        if (socket == nullptr) {
            throw std::runtime_error("zmq_socket(ZMQ_PUB) failed");
        }
        set_int_option(socket, ZMQ_LINGER, 0);
        set_int_option(socket, ZMQ_SNDHWM, kSocketHwm);
        set_int_option(socket, kZmqTcpNodelay, 1);
        if (zmq_bind(socket, endpoint_.c_str()) != 0) {
            throw std::runtime_error("zmq_bind failed");
        }

        alive_.store(true, std::memory_order_release);
        ready_.store(true, std::memory_order_release);

        while (running_.load(std::memory_order_relaxed)) {
            if (drain_available(socket)) {
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch (...) {
        worker_error_ = std::current_exception();
        ready_.store(true, std::memory_order_release);
    }

    alive_.store(false, std::memory_order_release);
    if (socket != nullptr) {
        zmq_close(socket);
    }
    if (ctx != nullptr) {
        zmq_ctx_term(ctx);
    }
}

void ThreadedZmqPub::start(double timeout_sec) {
    if (started_.load(std::memory_order_relaxed)) {
        return;
    }
    ready_.store(false, std::memory_order_relaxed);
    worker_error_ = nullptr;
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread([this] { worker(); });

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(timeout_sec));
    while (!ready_.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            running_.store(false, std::memory_order_relaxed);
            if (thread_.joinable()) {
                thread_.join();
            }
            throw std::runtime_error("zmq pub startup timed out");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (worker_error_) {
        running_.store(false, std::memory_order_relaxed);
        if (thread_.joinable()) {
            thread_.join();
        }
        std::rethrow_exception(worker_error_);
    }
    started_.store(true, std::memory_order_relaxed);
}

void ThreadedZmqPub::publish_nowait(std::span<const std::byte> payload) {
    if (!alive_.load(std::memory_order_acquire)) {
        throw std::runtime_error("zmq pub worker is not running");
    }
    ring_.try_push(payload);
}

void ThreadedZmqPub::close() {
    if (!started_.load(std::memory_order_relaxed)) {
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (ring_.size_approx() > 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    running_.store(false, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
    started_.store(false, std::memory_order_relaxed);
}

} // namespace swissquote::zmq
