#include "fixtures.hpp"

#include <swissquote/codec/codec.hpp>
#include <swissquote/codec/validate.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using swissquote::codec::encode_bbo_frame;
using swissquote::codec::flatten_bbo_frame;
using swissquote::codec::is_valid_bbo_row;
using swissquote::test::kSampleFetchTimeMs;
using swissquote::test::kSampleJson;
using swissquote::test::kSampleSymbol;

namespace {

std::span<const std::byte> as_bytes(std::string_view s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

} // namespace

TEST_CASE("validate accepts encode flatten rows", "[codec][validate]") {
    const auto frame = encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    const auto rows = flatten_bbo_frame(*frame);
    REQUIRE(rows.has_value());
    REQUIRE_FALSE(rows->empty());
    for (const auto& row : *rows) {
        CHECK(is_valid_bbo_row(row, row.ts_ms + 1'000));
    }
}

TEST_CASE("validate rejects skewed platform", "[codec][validate]") {
    swissquote::codec::BboRow row{
        .ts_ms = 1'782'941'100'045LL,
        .symbol = "EUR/USD",
        .platform = "apitalMarkets",
        .server = "Live7",
        .spread_profile = "premium",
        .bid_price = 1.1,
        .ask_price = 1.11,
        .bid_spread = 0.5F,
        .ask_spread = 0.5F,
    };
    CHECK_FALSE(is_valid_bbo_row(row, row.ts_ms + 1'000));
}

TEST_CASE("validate rejects xau fx-scale bid", "[codec][validate]") {
    swissquote::codec::BboRow row{
        .ts_ms = 1'782'941'100'045LL,
        .symbol = "XAU/USD",
        .platform = "SwissquoteCapitalMarkets",
        .server = "Live7",
        .spread_profile = "premium",
        .bid_price = 0.56,
        .ask_price = 0.57,
        .bid_spread = 0.5F,
        .ask_spread = 0.5F,
    };
    CHECK_FALSE(is_valid_bbo_row(row, row.ts_ms + 1'000));
}
