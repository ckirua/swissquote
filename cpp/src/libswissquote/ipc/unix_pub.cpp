#include "swissquote/ipc/unix_pub.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace swissquote::ipc {
namespace {

int make_unix_socket(int flags) {
    const int fd = ::socket(AF_UNIX, SOCK_SEQPACKET | flags, 0);
    if (fd < 0) {
        throw std::runtime_error(std::format("socket(AF_UNIX, SOCK_SEQPACKET) failed: {}", std::strerror(errno)));
    }
    return fd;
}

sockaddr_un make_address(const std::string& path) {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (path.empty() || path.size() >= sizeof(address.sun_path)) {
        throw std::invalid_argument("IPC socket path is empty or too long for AF_UNIX");
    }
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return address;
}

void set_nonblocking_cloexec(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error(std::format("fcntl(O_NONBLOCK) failed: {}", std::strerror(errno)));
    }
    const int fd_flags = ::fcntl(fd, F_GETFD, 0);
    if (fd_flags < 0 || ::fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC) < 0) {
        throw std::runtime_error(std::format("fcntl(FD_CLOEXEC) failed: {}", std::strerror(errno)));
    }
}

void remove_stale_socket(const std::string& path, const sockaddr_un& address) {
    struct stat info {};
    if (::lstat(path.c_str(), &info) != 0) {
        if (errno == ENOENT) {
            return;
        }
        throw std::runtime_error(std::format("lstat({}) failed: {}", path, std::strerror(errno)));
    }
    if (!S_ISSOCK(info.st_mode)) {
        throw std::runtime_error(std::format("IPC path exists and is not a socket: {}", path));
    }

    const int probe = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (probe < 0) {
        throw std::runtime_error(std::format("socket probe failed: {}", std::strerror(errno)));
    }
    const int connected = ::connect(
        probe,
        reinterpret_cast<const sockaddr*>(&address),
        static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + std::strlen(address.sun_path) + 1));
    const int connect_error = errno;
    ::close(probe);
    if (connected == 0) {
        throw std::runtime_error(std::format("IPC socket is already accepting clients: {}", path));
    }
    if (connect_error != ECONNREFUSED && connect_error != ENOENT) {
        throw std::runtime_error(std::format("IPC socket probe failed: {}", std::strerror(connect_error)));
    }
    if (::unlink(path.c_str()) != 0 && errno != ENOENT) {
        throw std::runtime_error(std::format("unlink stale socket {} failed: {}", path, std::strerror(errno)));
    }
}

} // namespace

ThreadedUnixSeqpacketPub::ThreadedUnixSeqpacketPub(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

ThreadedUnixSeqpacketPub::~ThreadedUnixSeqpacketPub() {
    close();
}

bool ThreadedUnixSeqpacketPub::is_healthy() const {
    return alive_.load(std::memory_order_acquire) && started_.load(std::memory_order_relaxed);
}

PubStats ThreadedUnixSeqpacketPub::stats() const {
    return PubStats{
        .sent = sent_.load(std::memory_order_relaxed),
        .dropped_queue = ring_.dropped(),
        .dropped_no_client = dropped_no_client_.load(std::memory_order_relaxed),
        .dropped_clients = dropped_clients_.load(std::memory_order_relaxed),
        .send_errors = send_errors_.load(std::memory_order_relaxed),
        .queue_size = ring_.size_approx(),
        .client_count = client_count_.load(std::memory_order_relaxed),
    };
}

void ThreadedUnixSeqpacketPub::wake_worker() noexcept {
    const int fd = wake_fd_.load(std::memory_order_relaxed);
    if (fd < 0) {
        return;
    }
    const std::uint64_t one = 1;
    const auto ignored = ::write(fd, &one, sizeof(one));
    (void)ignored;
}

void ThreadedUnixSeqpacketPub::worker() {
    int listener = -1;
    int wake_fd = -1;
    bool bound = false;
    std::array<int, kMaxClients> clients{};
    clients.fill(-1);
    std::size_t client_count = 0;
    try {
        const auto address = make_address(socket_path_);
        const int created = make_unix_socket(SOCK_NONBLOCK | SOCK_CLOEXEC);
        listener = created;
        wake_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wake_fd < 0) {
            throw std::runtime_error(std::format("eventfd failed: {}", std::strerror(errno)));
        }
        wake_fd_.store(wake_fd, std::memory_order_release);
        remove_stale_socket(socket_path_, address);
        const auto address_length = static_cast<socklen_t>(
            offsetof(sockaddr_un, sun_path) + socket_path_.size() + 1);
        if (::bind(listener, reinterpret_cast<const sockaddr*>(&address), address_length) != 0) {
            throw std::runtime_error(std::format("bind({}) failed: {}", socket_path_, std::strerror(errno)));
        }
        bound = true;
        if (::chmod(socket_path_.c_str(), 0660) != 0) {
            throw std::runtime_error(std::format("chmod({}) failed: {}", socket_path_, std::strerror(errno)));
        }
        if (::listen(listener, static_cast<int>(kMaxClients)) != 0) {
            throw std::runtime_error(std::format("listen({}) failed: {}", socket_path_, std::strerror(errno)));
        }

        alive_.store(true, std::memory_order_release);
        ready_.store(true, std::memory_order_release);
        while (running_.load(std::memory_order_relaxed)) {
            pollfd wait_fds[2]{{.fd = listener, .events = POLLIN, .revents = 0},
                              {.fd = wake_fd, .events = POLLIN, .revents = 0}};
            const int poll_result = ::poll(wait_fds, 2, -1);
            if (poll_result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::runtime_error(std::format("poll failed: {}", std::strerror(errno)));
            }

            if ((wait_fds[0].revents & POLLIN) != 0) {
                while (true) {
                    const int client_fd = ::accept(listener, nullptr, nullptr);
                    if (client_fd < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        throw std::runtime_error(std::format("accept failed: {}", std::strerror(errno)));
                    }
                    try {
                        set_nonblocking_cloexec(client_fd);
                    } catch (...) {
                        ::close(client_fd);
                        throw;
                    }
                    if (client_count == clients.size()) {
                        ::close(client_fd);
                        continue;
                    }
                    clients[client_count++] = client_fd;
                    client_count_.store(client_count, std::memory_order_relaxed);
                }
            }

            if ((wait_fds[1].revents & POLLIN) != 0) {
                std::uint64_t value = 0;
                while (::read(wake_fd, &value, sizeof(value)) > 0) {}
                const std::byte* data = nullptr;
                std::size_t length = 0;
                while (ring_.try_peek_view(&data, &length)) {
                    if (client_count == 0) {
                        dropped_no_client_.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::size_t index = 0;
                        while (index < client_count) {
                            const auto sent = ::send(
                                clients[index],
                                data,
                                length,
                                MSG_DONTWAIT | MSG_NOSIGNAL);
                            if (sent == static_cast<ssize_t>(length)) {
                                ++index;
                                continue;
                            }
                            if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EPIPE
                                && errno != ECONNRESET) {
                                send_errors_.fetch_add(1, std::memory_order_relaxed);
                            } else {
                                dropped_clients_.fetch_add(1, std::memory_order_relaxed);
                            }
                            ::close(clients[index]);
                            clients[index] = clients[client_count - 1];
                            clients[client_count - 1] = -1;
                            --client_count;
                            client_count_.store(client_count, std::memory_order_relaxed);
                        }
                        if (client_count > 0) {
                            sent_.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    ring_.try_commit_pop();
                }
            }
        }
    } catch (...) {
        worker_error_ = std::current_exception();
        ready_.store(true, std::memory_order_release);
    }

    alive_.store(false, std::memory_order_release);
    client_count_.store(0, std::memory_order_relaxed);
    for (std::size_t i = 0; i < client_count; ++i) {
        ::close(clients[i]);
    }
    if (listener >= 0) {
        ::close(listener);
    }
    if (wake_fd >= 0) {
        wake_fd_.store(-1, std::memory_order_release);
        ::close(wake_fd);
    }
    if (bound) {
        ::unlink(socket_path_.c_str());
    }
}

void ThreadedUnixSeqpacketPub::start(double timeout_sec) {
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
            wake_worker();
            if (thread_.joinable()) {
                thread_.join();
            }
            throw std::runtime_error("Unix IPC publisher startup timed out");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (worker_error_) {
        running_.store(false, std::memory_order_relaxed);
        wake_worker();
        if (thread_.joinable()) {
            thread_.join();
        }
        std::rethrow_exception(worker_error_);
    }
    started_.store(true, std::memory_order_relaxed);
}

void ThreadedUnixSeqpacketPub::publish_nowait(std::span<const std::byte> payload) {
    if (!alive_.load(std::memory_order_acquire)) {
        throw std::runtime_error("Unix IPC publisher worker is not running");
    }
    bool pushed = false;
    {
        std::lock_guard lock(publish_mu_);
        pushed = ring_.try_push(payload);
    }
    if (pushed) {
        wake_worker();
    }
}

void ThreadedUnixSeqpacketPub::close() {
    if (!started_.load(std::memory_order_relaxed)) {
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (ring_.size_approx() > 0 && std::chrono::steady_clock::now() < deadline) {
        wake_worker();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    running_.store(false, std::memory_order_relaxed);
    wake_worker();
    if (thread_.joinable()) {
        thread_.join();
    }
    started_.store(false, std::memory_order_relaxed);
}

} // namespace swissquote::ipc
