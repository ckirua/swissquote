#include "swissquote/zmq/sub.hpp"

#include "swissquote/zmq/zmq_io.hpp"

#include <chrono>
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

void subscribe_all(void* socket) {
    if (zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0) != 0) {
        throw std::runtime_error("zmq_setsockopt(ZMQ_SUBSCRIBE) failed");
    }
}

} // namespace

ThreadedZmqSub::ThreadedZmqSub(std::string endpoint) : endpoint_(std::move(endpoint)) {}

ThreadedZmqSub::~ThreadedZmqSub() {
    close();
}

bool ThreadedZmqSub::is_healthy() const {
    return alive_.load(std::memory_order_acquire) && started_.load(std::memory_order_relaxed);
}

SubStats ThreadedZmqSub::stats() const {
    return SubStats{
        .received = received_.load(std::memory_order_relaxed),
        .dropped_queue = ring_.dropped(),
        .recv_errors = recv_errors_.load(std::memory_order_relaxed),
        .queue_size = ring_.size_approx(),
    };
}

bool ThreadedZmqSub::recv_available(void* socket) {
    if (pending_push_) {
        if (ring_.try_push(pending_frame_)) {
            received_.fetch_add(1, std::memory_order_relaxed);
            pending_push_ = false;
            pending_frame_.clear();
            wake_cv_.notify_all();
        } else {
            return false;
        }
    }

    bool got = false;
    bool keep_recv = true;
    while (keep_recv && running_.load(std::memory_order_relaxed)) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        const int rc = io::try_recv_frame(socket, &msg);
        if (rc == 0) {
            const std::size_t msg_size = zmq_msg_size(&msg);
            const char* msg_data = static_cast<const char*>(zmq_msg_data(&msg));
            if (ring_.try_push_bytes(msg_data, msg_size)) {
                received_.fetch_add(1, std::memory_order_relaxed);
                wake_cv_.notify_all();
                got = true;
            } else {
                pending_frame_.resize(msg_size);
                if (msg_size > 0) {
                    std::memcpy(pending_frame_.data(), msg_data, msg_size);
                }
                pending_push_ = true;
                got = true;
                keep_recv = false;
            }
            zmq_msg_close(&msg);
        } else if (rc == 1) {
            zmq_msg_close(&msg);
            keep_recv = false;
        } else {
            zmq_msg_close(&msg);
            recv_errors_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return got;
}

void ThreadedZmqSub::worker() {
    void* ctx = zmq_ctx_new();
    void* socket = nullptr;
    try {
        if (ctx == nullptr) {
            throw std::runtime_error("zmq_ctx_new failed");
        }
        socket = zmq_socket(ctx, ZMQ_SUB);
        if (socket == nullptr) {
            throw std::runtime_error("zmq_socket(ZMQ_SUB) failed");
        }
        set_int_option(socket, ZMQ_LINGER, 0);
        set_int_option(socket, ZMQ_RCVHWM, kSocketHwm);
        set_int_option(socket, kZmqTcpNodelay, 1);
        subscribe_all(socket);
        if (zmq_connect(socket, endpoint_.c_str()) != 0) {
            throw std::runtime_error("zmq_connect failed");
        }

        alive_.store(true, std::memory_order_release);
        ready_.store(true, std::memory_order_release);

        while (running_.load(std::memory_order_relaxed)) {
            if (recv_available(socket)) {
                continue;
            }
            io::poll_one(socket, ZMQ_POLLIN, 1);
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

void ThreadedZmqSub::start(double timeout_sec) {
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
            wake_cv_.notify_all();
            if (thread_.joinable()) {
                thread_.join();
            }
            throw std::runtime_error("zmq sub startup timed out");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (worker_error_) {
        running_.store(false, std::memory_order_relaxed);
        wake_cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
        std::rethrow_exception(worker_error_);
    }
    started_.store(true, std::memory_order_relaxed);
}

std::optional<std::vector<std::byte>> ThreadedZmqSub::try_pop_frame() {
    return ring_.try_pop();
}

std::optional<std::vector<std::byte>> ThreadedZmqSub::pop_wait_frame(double timeout_sec) {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(timeout_sec));
    while (true) {
        if (auto frame = ring_.try_pop()) {
            return frame;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return std::nullopt;
        }
        std::unique_lock lock(wake_mu_);
        wake_cv_.wait_for(lock, std::chrono::milliseconds(1));
    }
}

void ThreadedZmqSub::close() {
    if (!started_.load(std::memory_order_relaxed)) {
        return;
    }
    running_.store(false, std::memory_order_relaxed);
    wake_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    started_.store(false, std::memory_order_relaxed);
}

} // namespace swissquote::zmq
