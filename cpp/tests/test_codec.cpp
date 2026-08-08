#include "fixtures.hpp"

#include <swissquote/codec/codec.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

#include <cstring>

using swissquote::codec::encode_bbo_frame;
using swissquote::codec::flatten_bbo_frame;
using swissquote::codec::flatten_bbo_frame_views;
using swissquote::test::kSampleFetchTimeMs;
using swissquote::test::kSampleFrameLen;
using swissquote::test::kSampleJson;
using swissquote::test::kSampleSymbol;

namespace {

std::span<const std::byte> as_bytes(std::string_view s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

} // namespace

TEST_CASE("encode produces expected frame length", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    CHECK(frame->size() == kSampleFrameLen);
}

TEST_CASE("encode flatten roundtrip", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    const auto rows = flatten_bbo_frame(*frame);
    REQUIRE(rows.has_value());
    CHECK(rows->size() == 9);
    CHECK((*rows)[0].symbol == "EUR/USD");
    CHECK((*rows)[0].spread_profile == "premium");
    CHECK_THAT((*rows)[0].bid_price, WithinAbs(1.13768, 1e-6));
}

TEST_CASE("view flatten matches owning flatten", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());

    const auto owned_rows = flatten_bbo_frame(*frame);
    REQUIRE(owned_rows.has_value());
    REQUIRE_FALSE(owned_rows->empty());

    const auto view_rows = flatten_bbo_frame_views(*frame);
    REQUIRE(view_rows.has_value());
    REQUIRE(view_rows->size() == owned_rows->size());

    const auto& owned = owned_rows->front();
    const auto& view = view_rows->front();
    CHECK(view.ts_ms == owned.ts_ms);
    CHECK(view.symbol == owned.symbol);
    CHECK(view.platform == owned.platform);
    CHECK(view.server == owned.server);
    CHECK(view.spread_profile == owned.spread_profile);
    CHECK_THAT(view.bid_price, WithinAbs(owned.bid_price, 1e-12));
    CHECK_THAT(view.ask_price, WithinAbs(owned.ask_price, 1e-12));
    CHECK_THAT(view.bid_spread, WithinAbs(owned.bid_spread, 1e-6));
    CHECK_THAT(view.ask_spread, WithinAbs(owned.ask_spread, 1e-6));
}

TEST_CASE("view flatten strings remain valid while frame is alive", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());

    const auto view_rows = flatten_bbo_frame_views(*frame);
    REQUIRE(view_rows.has_value());
    REQUIRE_FALSE(view_rows->empty());

    // The encoded frame remains alive while every string_view is inspected.
    const auto& first = view_rows->front();
    CHECK(first.symbol == kSampleSymbol);
    CHECK(first.platform == "SwissquoteCapitalMarkets");
    CHECK(first.server == "Live7");
    CHECK(first.spread_profile == "premium");
}

TEST_CASE("view flatten rejects a truncated frame", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    REQUIRE(frame->size() > 1);

    auto truncated = *frame;
    truncated.resize(truncated.size() - 1);
    const auto view_rows = flatten_bbo_frame_views(truncated);
    CHECK_FALSE(view_rows.has_value());
}

TEST_CASE("encode rejects non-array json", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(R"({"x":1})"), 0);
    CHECK_FALSE(frame.has_value());
}

TEST_CASE("encode empty array", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes("[]"), 0);
    REQUIRE(frame.has_value());
    const auto rows = flatten_bbo_frame(*frame);
    REQUIRE(rows.has_value());
    CHECK(rows->empty());
}
