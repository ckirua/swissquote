#include "ipc/shared_memory_spsc_ring.hpp"

#include <swissquote/ipc/unix_pub.hpp>
#include <swissquote/ipc/unix_sub.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <span>
#include <memory>

#include <sys/wait.h>
#include <unistd.h>

namespace {
using Ring = swissquote::benchmark::ipc::SharedMemorySpscRing;
struct Envelope { std::uint64_t sequence; std::uint64_t sent_ns; };
std::uint64_t monotonic_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::vector<std::byte> make_payload(std::size_t total_size) {
    if (total_size < sizeof(Envelope)) throw std::invalid_argument("frame size too small");
    std::vector<std::byte> frame(total_size);
    for (std::size_t i = sizeof(Envelope); i < frame.size(); ++i)
        frame[i] = static_cast<std::byte>((i * 31U + 7U) & 0xffU);
    return frame;
}
bool verify_payload(std::span<const std::byte> frame) {
    for (std::size_t i = sizeof(Envelope); i < frame.size(); ++i) {
        if (frame[i] != static_cast<std::byte>((i * 31U + 7U) & 0xffU)) return false;
    }
    return true;
}
int child_consume(const std::string& mode, const std::string& endpoint, std::size_t count,
                  std::size_t frame_size, int result_fd) {
    std::vector<std::int64_t> latency;
    latency.reserve(count);
    std::vector<std::byte> frame(frame_size);
    std::size_t got = 0;
    try {
        if (mode == "ring") {
            auto ring = Ring::attach(endpoint);
            while (got < count) {
                std::size_t bytes = 0;
                if (ring.consume_into(frame.data(), frame.size(), bytes)) {
                    if (bytes != frame_size || !verify_payload(frame)) return 3;
                    Envelope envelope{};
                    std::memcpy(&envelope, frame.data(), sizeof(envelope));
                    if (envelope.sequence != got) return 4;
                    latency.push_back(static_cast<std::int64_t>(monotonic_ns() - envelope.sent_ns));
                    ++got;
                } else {
                    const auto sequence = ring.notification_sequence();
                    if (ring.empty()) ring.wait_for_data(sequence);
                }
            }
        } else {
            swissquote::ipc::ThreadedUnixSeqpacketSub sub(endpoint);
            sub.start();
            while (got < count) {
                auto received = sub.pop_wait_frame(10.0);
                if (!received) return 5;
                if (received->size() != frame_size || !verify_payload(*received)) return 6;
                Envelope envelope{};
                std::memcpy(&envelope, received->data(), sizeof(envelope));
                if (envelope.sequence != got) return 7;
                latency.push_back(static_cast<std::int64_t>(monotonic_ns() - envelope.sent_ns));
                ++got;
            }
        }
    } catch (...) { return 8; }
    std::sort(latency.begin(), latency.end());
    std::array<std::uint64_t, 3> result{
        got,
        static_cast<std::uint64_t>(latency[latency.size() / 2]),
        static_cast<std::uint64_t>(latency[(latency.size() * 99) / 100])};
    if (::write(result_fd, result.data(), sizeof(result)) != sizeof(result)) return 9;
    return 0;
}
void run_case(const std::string& mode, std::size_t frame_size, std::size_t count, int repetition) {
    const std::string endpoint = mode == "ring"
        ? std::format("/swissquote-bench-{}-{}", getpid(), repetition)
        : std::format("/tmp/swissquote-bench-{}-{}.sock", getpid(), repetition);
    Ring ring = mode == "ring" ? Ring::create(endpoint) : Ring::create("/swissquote-bench-unused-" + std::to_string(getpid()));
    int pipes[2];
    if (::pipe(pipes) != 0) throw std::runtime_error("pipe failed");
    const pid_t child = ::fork();
    if (child < 0) throw std::runtime_error("fork failed");
    if (child == 0) {
        ::close(pipes[0]);
        const int result = child_consume(mode, endpoint, count, frame_size, pipes[1]);
        ::_exit(result);
    }
    ::close(pipes[1]);
    std::unique_ptr<swissquote::ipc::ThreadedUnixSeqpacketPub> pub;
    if (mode == "uds") {
        pub = std::make_unique<swissquote::ipc::ThreadedUnixSeqpacketPub>(endpoint);
        pub->start();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (pub->stats().client_count == 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (pub->stats().client_count == 0) throw std::runtime_error("benchmark subscriber did not connect");
    }
    std::vector<std::byte> payload = make_payload(frame_size);
    const std::size_t burst = 87;
    for (std::size_t seq = 0; seq < count; ++seq) {
        Envelope envelope{seq, monotonic_ns()};
        std::memcpy(payload.data(), &envelope, sizeof(envelope));
        if (mode == "ring") {
            std::byte* slot = nullptr;
            while ((slot = ring.claim()) == nullptr) std::this_thread::yield();
            std::memcpy(slot, payload.data(), payload.size());
            if (!ring.publish(payload.size())) throw std::runtime_error("ring publish rejected frame");
        } else {
            while (pub->stats().queue_size >= swissquote::ipc::ThreadedUnixSeqpacketPub::kRingCapacity - 1)
                std::this_thread::yield();
            pub->publish_nowait(payload);
        }
        if (mode == "uds" && ((seq + 1) % burst == 0 || seq + 1 == count)) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (pub->stats().sent < seq + 1
                   && pub->stats().client_count > 0
                   && std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();
            if (pub->stats().sent < seq + 1)
                throw std::runtime_error("UDS failed to deliver all frames within 10 seconds");
        }
        if ((seq + 1) % burst == 0) std::this_thread::yield();
    }
    std::array<std::uint64_t, 3> result{};
    if (::read(pipes[0], result.data(), sizeof(result)) != sizeof(result))
        throw std::runtime_error("consumer did not report complete run");
    int status = 0;
    if (::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("consumer failed or observed corrupt/lost frames");
    if (pub) pub->close();
    ::close(pipes[0]);
    if (mode == "uds") Ring::unlink(ring.name());
    std::cout << mode << " bytes=" << frame_size << " repetition=" << repetition
              << " frames=" << result[0] << " drops=0 p50_ns=" << result[1]
              << " p99_ns=" << result[2] << '\n';
}
}

int main(int argc, char** argv) {
    std::size_t repetitions = 5, count = 100000;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string option(argv[i]);
        const auto value = static_cast<std::size_t>(std::stoull(argv[i + 1]));
        if (option == "--repetitions") repetitions = value;
        else if (option == "--frame-count") count = value;
        else if (option == "--burst-size") {
            if (value != 87) {
                std::cerr << "benchmark contract fixes burst-size at 87\n"; return 2;
            }
        } else { std::cerr << "unknown argument: " << option << '\n'; return 2; }
    }
    try {
        for (std::size_t rep = 0; rep < repetitions; ++rep) {
            for (const auto bytes : {std::size_t{448}, std::size_t{65536}}) {
                run_case("uds", bytes, count, static_cast<int>(rep));
                run_case("ring", bytes, count, static_cast<int>(rep));
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "benchmark failed: " << error.what() << '\n'; return 1;
    }
}
