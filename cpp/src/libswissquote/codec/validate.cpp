#include "swissquote/codec/validate.hpp"

#include <chrono>
#include <string_view>
#include <unordered_set>

namespace swissquote::codec {
namespace {

constexpr std::int64_t kMinQuoteTsMs = 1577836800000LL; // 2020-01-01 UTC
constexpr std::int64_t kMaxQuoteSkewMs = 86'400'000LL;  // 1 day

bool is_known_platform(std::string_view platform) {
    static const std::unordered_set<std::string_view> kPlatforms = {
        "AT",
        "SwissquoteCapitalMarkets",
        "SwissquoteLtd",
    };
    return kPlatforms.contains(platform);
}

bool is_known_server(std::string_view server) {
    static const std::unordered_set<std::string_view> kServers = {
        "AT",
        "Live5",
        "Live7",
    };
    return kServers.contains(server);
}

double metal_min_bid(std::string_view symbol) {
    if (symbol.starts_with("XAU/") || symbol.starts_with("XPD/") || symbol.starts_with("XPT/")) {
        return 1000.0;
    }
    if (symbol.starts_with("XAG/")) {
        return 1.0;
    }
    return 0.0;
}

} // namespace

template <typename Row>
bool is_valid_bbo_row_impl(const Row& row, std::int64_t now_ms) {
    if (!is_known_platform(row.platform) || !is_known_server(row.server)) {
        return false;
    }
    if (row.symbol.empty() || row.symbol.find('/') == std::string_view::npos) {
        return false;
    }
    if (row.ts_ms < kMinQuoteTsMs || row.ts_ms > now_ms + kMaxQuoteSkewMs) {
        return false;
    }
    if (row.bid_price <= 0.0 || row.ask_price <= 0.0 || row.bid_price > row.ask_price) {
        return false;
    }
    const double floor = metal_min_bid(row.symbol);
    if (floor > 0.0 && row.bid_price < floor) {
        return false;
    }
    return true;
}

bool is_valid_bbo_row(const BboRow& row, std::int64_t now_ms) {
    return is_valid_bbo_row_impl(row, now_ms);
}

bool is_valid_bbo_row(const BboRowView& row, std::int64_t now_ms) {
    return is_valid_bbo_row_impl(row, now_ms);
}

} // namespace swissquote::codec
