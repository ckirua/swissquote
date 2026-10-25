#include <swissquote/codec/codec.hpp>
#include <swissquote/ipc/spsc_ring.hpp>
#include <swissquote/ipc/unix_pub.hpp>
#include <swissquote/ipc/unix_sub.hpp>

#include "fixtures.hpp"

#include "../benchmarks/ipc/shared_memory_spsc_ring.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sys/wait.h>




#include <chrono>
#include <cstddef>
#include <cstring>
#include <format>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using swissquote::test::kSampleFetchTimeMs;
using swissquote::test::kSampleJson;
using swissquote::test::kSampleSymbol;

TEST_CASE("Unix seqpacket publisher fans out frames and subscribers reconnect") {
    const std::string socket_path =
        std::format("/tmp/swissquote-ipc-test-{}.sock", static_cast<int>(getpid()));
    swissquote::ipc::ThreadedUnixSeqpacketPub pub(socket_path);
    pub.start();
    swissquote::ipc::ThreadedUnixSeqpacketSub first(socket_path);
    swissquote::ipc::ThreadedUnixSeqpacketSub second(socket_path);
    first.start();
    second.start();

    const auto wait_for_clients = [](const auto& publisher, std::size_t expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            if (publisher.stats().client_count == expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };
    REQUIRE(wait_for_clients(pub, 2));

    const auto frame = swissquote::codec::encode_bbo_frame(
        kSampleSymbol,
        {reinterpret_cast<const std::byte*>(kSampleJson.data()), kSampleJson.size()},
        kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    pub.publish_nowait(*frame);

    bool first_received = false;
    bool second_received = false;
    REQUIRE(first.consume_wait_frame(1.0, [&](std::span<const std::byte> payload) {
        first_received = payload.size() == frame->size()
            && std::memcmp(payload.data(), frame->data(), frame->size()) == 0;
    }));
    REQUIRE(second.consume_wait_frame(1.0, [&](std::span<const std::byte> payload) {
        second_received = payload.size() == frame->size()
            && std::memcmp(payload.data(), frame->data(), frame->size()) == 0;
    }));
    CHECK(first_received);
    CHECK(second_received);

    pub.close();
    swissquote::ipc::ThreadedUnixSeqpacketPub restarted_pub(socket_path);
    restarted_pub.start();
    REQUIRE(wait_for_clients(restarted_pub, 2));
    restarted_pub.publish_nowait(*frame);
    CHECK(first.consume_wait_frame(1.0, [&](std::span<const std::byte> payload) {
        CHECK(payload.size() == frame->size());
        CHECK(std::memcmp(payload.data(), frame->data(), frame->size()) == 0);
    }));
    CHECK(second.consume_wait_frame(1.0, [&](std::span<const std::byte> payload) {
        CHECK(payload.size() == frame->size());
        CHECK(std::memcmp(payload.data(), frame->data(), frame->size()) == 0);
    }));

    restarted_pub.close();
    first.close();
    second.close();
}

TEST_CASE("SPSC byte ring preserves frames across capacity and slot reuse") {
    swissquote::ipc::SpscBytesRing ring(2, 8);
    const std::string first{"abc"};
    const std::string second{"12345678"};
    const std::string third{"x"};
    CHECK(ring.try_push_bytes(first.data(), first.size()));
    CHECK(ring.try_push_bytes(second.data(), second.size()));
    CHECK_FALSE(ring.try_push_bytes(third.data(), third.size()));
    CHECK(ring.dropped() == 1);

    const std::byte* payload = nullptr;
    std::size_t length = 0;
    REQUIRE(ring.try_peek_view(&payload, &length));
    CHECK(length == first.size());
    CHECK(std::memcmp(payload, first.data(), length) == 0);
    CHECK(ring.try_commit_pop());

    CHECK(ring.try_push_bytes(third.data(), third.size()));
    REQUIRE(ring.try_peek_view(&payload, &length));
    CHECK(length == second.size());
    CHECK(std::memcmp(payload, second.data(), length) == 0);
    CHECK(ring.try_commit_pop());
    REQUIRE(ring.try_peek_view(&payload, &length));
    CHECK(length == third.size());
    CHECK(std::memcmp(payload, third.data(), length) == 0);
    CHECK(ring.try_commit_pop());
    CHECK(ring.size_approx() == 0);
}

TEST_CASE("Shared-memory SPSC ring preserves bounded FIFO frames across processes") {
    using Ring = swissquote::benchmark::ipc::SharedMemorySpscRing;
    const std::string name = std::format("/swissquote-ring-{}-{}", getpid(),
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto producer = Ring::create(name);
    const pid_t child = ::fork();
    REQUIRE(child >= 0);
    if (child == 0) {
        try {
            auto consumer = Ring::attach(name);
            std::vector<std::byte> output(Ring::kMaxPayload);
            std::size_t length = 0;
            for (std::uint32_t expected = 0; expected < 2050; ++expected) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (!consumer.consume_into(output.data(), output.size(), length)) {
                    if (std::chrono::steady_clock::now() >= deadline) _exit(2);
                    const auto sequence = consumer.notification_sequence();
                    if (consumer.empty()) consumer.wait_for_data(sequence);
                }
                if (length != sizeof(expected)
                    || std::memcmp(output.data(), &expected, sizeof(expected)) != 0) _exit(3);
            }
            _exit(0);
        } catch (...) { _exit(4); }
    }
    for (std::uint32_t value = 0; value < 2050; ++value) {
        std::byte* slot = nullptr;
        while ((slot = producer.claim()) == nullptr) std::this_thread::yield();
        std::memcpy(slot, &value, sizeof(value));
        REQUIRE(producer.publish(sizeof(value)));
    }
    int status = 0;
    REQUIRE(::waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    CHECK(Ring::unlink(name));
    CHECK_THROWS(Ring::attach(name));
}

TEST_CASE("Shared-memory SPSC ring rejects full and oversize without overwriting") {
    using Ring = swissquote::benchmark::ipc::SharedMemorySpscRing;
    const std::string name = std::format("/swissquote-ring-{}-{}", getpid(),
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto ring = Ring::create(name);
    std::vector<std::byte> output(Ring::kMaxPayload);
    for (std::uint32_t i = 0; i < Ring::kCapacity; ++i) {
        auto* slot = ring.claim();
        REQUIRE(slot != nullptr);
        std::memcpy(slot, &i, sizeof(i));
        REQUIRE(ring.publish(sizeof(i)));
    }
    CHECK(ring.claim() == nullptr);
    std::size_t length = 0;
    REQUIRE(ring.consume_into(output.data(), output.size(), length));
    std::uint32_t first = UINT32_MAX;
    std::memcpy(&first, output.data(), sizeof(first));
    CHECK(first == 0);
    auto* slot = ring.claim();
    REQUIRE(slot != nullptr);
    std::memset(slot, 0x5a, Ring::kMaxPayload);
    CHECK_FALSE(ring.publish(Ring::kMaxPayload + 1));
    REQUIRE(ring.publish(Ring::kMaxPayload));
    for (std::uint32_t i = 1; i < Ring::kCapacity; ++i) {
        REQUIRE(ring.consume_into(output.data(), output.size(), length));
        std::uint32_t value = UINT32_MAX;
        std::memcpy(&value, output.data(), sizeof(value));
        CHECK(value == i);
    }
    REQUIRE(ring.consume_into(output.data(), output.size(), length));
    CHECK(length == Ring::kMaxPayload);
    CHECK(output.front() == std::byte{0x5a});
    CHECK(Ring::unlink(name));
}
