#include "fixtures.hpp"

#include <swissquote/codec/codec.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

#include <cstdint>

using swissquote::codec::DecodeError;
using swissquote::codec::encode_bbo_frame;
using swissquote::codec::encode_bbo_frame_into;
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

std::uint32_t read_u32_le(const std::vector<std::byte>& frame, std::size_t offset) {
    return static_cast<std::uint32_t>(std::to_integer<unsigned char>(frame[offset]))
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(frame[offset + 1])) << 8)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(frame[offset + 2])) << 16)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(frame[offset + 3])) << 24);
}

void write_u32_le(std::vector<std::byte>& frame, std::size_t offset, std::uint32_t value) {
    frame[offset] = static_cast<std::byte>(value & 0xff);
    frame[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
    frame[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
    frame[offset + 3] = static_cast<std::byte>(value >> 24);
}

void write_u16_le(std::vector<std::byte>& frame, std::size_t offset, std::uint16_t value) {
    frame[offset] = static_cast<std::byte>(value & 0xff);
    frame[offset + 1] = static_cast<std::byte>(value >> 8);
}

std::size_t venue_group_offset(const std::vector<std::byte>& frame) {
    constexpr std::size_t root_offset = 8;
    constexpr std::size_t root_block_length = 12;
    return root_offset + root_block_length + read_u32_le(frame, root_offset);
}

std::size_t profile_group_offset(const std::vector<std::byte>& frame) {
    const auto venue_offset = venue_group_offset(frame);
    const auto venue_fixed_offset = venue_offset + 4;
    const auto platform_length = read_u32_le(frame, venue_fixed_offset);
    const auto server_length = read_u32_le(frame, venue_fixed_offset + 4);
    return venue_fixed_offset + 16 + platform_length + server_length;
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

TEST_CASE("view flatten rejects an invalid venue group block length", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());

    auto malformed = *frame;
    write_u16_le(malformed, venue_group_offset(malformed), 0);
    const auto rows = flatten_bbo_frame_views(malformed);
    REQUIRE_FALSE(rows.has_value());
    CHECK(rows.error() == DecodeError::InvalidHeader);
}

TEST_CASE("view flatten rejects an invalid profile group block length", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());

    auto malformed = *frame;
    write_u16_le(malformed, profile_group_offset(malformed), 0);
    const auto rows = flatten_bbo_frame_views(malformed);
    REQUIRE_FALSE(rows.has_value());
    CHECK(rows.error() == DecodeError::InvalidHeader);
}

TEST_CASE("view flatten rejects variable data beyond the frame", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());

    auto malformed = *frame;
    write_u32_le(malformed, 8, 0xffffffffU);
    const auto rows = flatten_bbo_frame_views(malformed);
    CHECK_FALSE(rows.has_value());
}

TEST_CASE("view flatten rejects non-ASCII variable data", "[codec]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());

    auto malformed = *frame;
    malformed[8 + 12] = static_cast<std::byte>(0x80);
    const auto rows = flatten_bbo_frame_views(malformed);
    REQUIRE_FALSE(rows.has_value());
    CHECK(rows.error() == DecodeError::InvalidAscii);
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

TEST_CASE("generated SBE codec preserves empty variable fields", "[codec]") {
    const auto frame = encode_bbo_frame("", as_bytes("[]"), 0);
    REQUIRE(frame.has_value());
    CHECK(frame->size() == 24);

    const auto rows = flatten_bbo_frame_views(*frame);
    REQUIRE(rows.has_value());
    CHECK(rows->empty());
}

TEST_CASE("reusable codec buffers preserve frame and row results", "[codec]") {
    swissquote::codec::EncodeWorkspace workspace;
    REQUIRE(encode_bbo_frame_into(
                kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs, workspace)
        .has_value());
    const auto encoded_capacity = workspace.output.capacity();
    const auto expected = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(expected.has_value());
    CHECK(workspace.output == *expected);

    std::vector<swissquote::codec::BboRowView> rows;
    REQUIRE(flatten_bbo_frame_views(workspace.output, rows).has_value());
    CHECK(rows.size() == 9);
    const auto rows_capacity = rows.capacity();
    REQUIRE(flatten_bbo_frame_views(workspace.output, rows).has_value());
    CHECK(rows.capacity() == rows_capacity);
    REQUIRE(encode_bbo_frame_into(
                kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs, workspace)
        .has_value());
    CHECK(workspace.output.capacity() == encoded_capacity);
}

TEST_CASE("encode rejects variable fields beyond SBE limits", "[codec]") {
    const std::string oversized_symbol(65537, 'x');
    const auto frame = encode_bbo_frame(oversized_symbol, as_bytes("[]"), 0);
    CHECK_FALSE(frame.has_value());
}

TEST_CASE("encode rejects non-ASCII variable fields", "[codec]") {
    const std::string invalid_symbol{"A\200"};
    const auto frame = encode_bbo_frame(invalid_symbol, as_bytes("[]"), 0);
    CHECK_FALSE(frame.has_value());
}
