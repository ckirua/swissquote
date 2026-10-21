#include "swissquote/ipc/unix_sub.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <thread>
#include <utility>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>

namespace swissquote::ipc {
namespace {

sockaddr_un make_address(const std::string& path) {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (path.empty() || path.size() >= sizeof(address.sun_path)) {
        throw std::invalid_argument("IPC socket path is empty or too long for AF_UNIX");
    }
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return address;
}

int connect_socket(const std::string& path, const sockaddr_un& address) {
    const int fd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw std::runtime_error(std::format("socket(AF_UNIX, SOCK_SEQPACKET) failed: {}", std::strerror(errno)));
    }
    const auto address_length = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + path.size() + 1);
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&address), address_length) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

} // namespace

ThreadedUnixSeqpacketSub::ThreadedUnixSeqpacketSub(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

ThreadedUnixSeqpacketSub::~ThreadedUnixSeqpacketSub() {
    close();
}

bool ThreadedUnixSeqpacketSub::is_healthy() const {
    return alive_.load(std::memory_order_acquire) && started_.load(std::memory_order_relaxed);
}

SubStats ThreadedUnixSeqpacketSub::stats() const {
    return SubStats{
        .received = received_.load(std::memory_order_relaxed),
        .dropped_queue = ring_.dropped(),
        .recv_errors = recv_errors_.load(std::memory_order_relaxed),
        .queue_size = ring_.size_approx(),
    };
}

void ThreadedUnixSeqpacketSub::worker() {
    int socket_fd = -1;
    bool ready = false;
    try {
        const auto address = make_address(socket_path_);
        std::vector<std::byte> receive_buffer(kMaxPayload);
        std::vector<std::byte> pending_frame;
        alive_.store(true, std::memory_order_release);
        ready_.store(true, std::memory_order_release);
        ready = true;

        while (running_.load(std::memory_order_relaxed)) {
            if (socket_fd < 0) {
                socket_fd = connect_socket(socket_path_, address);
                if (socket_fd < 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
            }

            if (!pending_frame.empty()) {
                if (!ring_.try_push(pending_frame)) {
                    pollfd wait_fd{.fd = socket_fd, .events = 0, .revents = 0};
                    const int wait_result = ::poll(&wait_fd, 1, 1);
                    if (wait_result < 0 && errno != EINTR) {
                        recv_errors_.fetch_add(1, std::memory_order_relaxed);
                        ::close(socket_fd);
                        socket_fd = -1;
                    }
                    continue;
                }
                pending_frame.clear();
                received_.fetch_add(1, std::memory_order_relaxed);
                {
                    std::lock_guard lock(wake_mu_);
                    wake_cv_.notify_all();
                }
            }

            pollfd wait_fd{.fd = socket_fd, .events = POLLIN, .revents = 0};
            const int poll_result = ::poll(&wait_fd, 1, 100);
            if (poll_result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                recv_errors_.fetch_add(1, std::memory_order_relaxed);
                ::close(socket_fd);
                socket_fd = -1;
                continue;
            }
            if (poll_result == 0) {
                continue;
            }
            if ((wait_fd.revents & POLLIN) != 0) {
                iovec payload{.iov_base = receive_buffer.data(), .iov_len = receive_buffer.size()};
                msghdr message{};
                message.msg_iov = &payload;
                message.msg_iovlen = 1;
                const auto received = ::recvmsg(socket_fd, &message, MSG_DONTWAIT);
                if (received > 0 && (message.msg_flags & MSG_TRUNC) == 0) {
                    const auto length = static_cast<std::size_t>(received);
                    if (ring_.try_push_bytes(
                            reinterpret_cast<const char*>(receive_buffer.data()), length)) {
                        received_.fetch_add(1, std::memory_order_relaxed);
                        {
                            std::lock_guard lock(wake_mu_);
                            wake_cv_.notify_all();
                        }
                    } else {
                        pending_frame.assign(receive_buffer.begin(), receive_buffer.begin() + received);
                    }
                } else if (received == 0 || (message.msg_flags & MSG_TRUNC) != 0) {
                    recv_errors_.fetch_add(1, std::memory_order_relaxed);
                    ::close(socket_fd);
                    socket_fd = -1;
                } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    recv_errors_.fetch_add(1, std::memory_order_relaxed);
                    ::close(socket_fd);
                    socket_fd = -1;
                }
            }
            if (socket_fd >= 0 && (wait_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                recv_errors_.fetch_add(1, std::memory_order_relaxed);
                ::close(socket_fd);
                socket_fd = -1;
            }
        }
    } catch (...) {
        worker_error_ = std::current_exception();
        ready_.store(true, std::memory_order_release);
        ready = true;
    }

    if (!ready) {
        ready_.store(true, std::memory_order_release);
    }
    alive_.store(false, std::memory_order_release);
    if (socket_fd >= 0) {
        ::close(socket_fd);
    }
    {
        std::lock_guard lock(wake_mu_);
        wake_cv_.notify_all();
    }
}

void ThreadedUnixSeqpacketSub::start(double timeout_sec) {
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
            throw std::runtime_error("Unix IPC subscriber startup timed out");
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

std::optional<std::vector<std::byte>> ThreadedUnixSeqpacketSub::try_pop_frame() {
    return ring_.try_pop();
}

std::optional<std::vector<std::byte>> ThreadedUnixSeqpacketSub::pop_wait_frame(double timeout_sec) {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(timeout_sec));
    std::unique_lock lock(wake_mu_);
    if (!wake_cv_.wait_until(lock, deadline, [this] {
            return ring_.size_approx() > 0 || !running_.load(std::memory_order_relaxed);
        })) {
        return std::nullopt;
    }
    lock.unlock();
    return ring_.try_pop();
}

void ThreadedUnixSeqpacketSub::close() {
    if (!started_.load(std::memory_order_relaxed)) {
        return;
    }
    running_.store(false, std::memory_order_relaxed);
    {
        std::lock_guard lock(wake_mu_);
        wake_cv_.notify_all();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    started_.store(false, std::memory_order_relaxed);
}

} // namespace swissquote::ipc
