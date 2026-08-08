#include <swissquote/codec/codec.hpp>
#include <swissquote/zmq/pub.hpp>
#include <swissquote/zmq/sub.hpp>

#include "fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <format>
#include <optional>
#include <thread>
#include <unistd.h>
#include <vector>

using swissquote::test::kSampleFetchTimeMs;
using swissquote::test::kSampleJson;
using swissquote::test::kSampleSymbol;

TEST_CASE("zmq pub sub roundtrip", "[zmq]") {
    const std::string endpoint =
        std::format("ipc:///tmp/swissquote-zmq-test-{}", static_cast<int>(getpid()));
    swissquote::zmq::ThreadedZmqPub pub(endpoint);
    swissquote::zmq::ThreadedZmqSub sub(endpoint);
    pub.start();
    sub.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const auto frame = swissquote::codec::encode_bbo_frame(
        kSampleSymbol,
        {reinterpret_cast<const std::byte*>(kSampleJson.data()), kSampleJson.size()},
        kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    pub.publish_nowait(*frame);

    bool received = false;
    for (int i = 0; i < 100 && !received; ++i) {
        received = sub.consume_wait_frame(0.05, [&](std::span<const std::byte> payload) {
            CHECK(payload.size() == frame->size());
            CHECK(std::memcmp(payload.data(), frame->data(), frame->size()) == 0);
        });
    }
    REQUIRE(received);

    pub.close();
    sub.close();
}
