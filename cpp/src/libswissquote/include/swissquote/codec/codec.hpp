#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "swissquote/codec/errors.hpp"

namespace swissquote::codec {

struct BboRow {
    std::int64_t ts_ms{};
    std::string symbol;
    std::string platform;
    std::string server;
    std::string spread_profile;
    double bid_price{};
    double ask_price{};
    float bid_spread{};
    float ask_spread{};
};

struct BboRowView {
    std::int64_t ts_ms{};
    std::string_view symbol;
    std::string_view platform;
    std::string_view server;
    std::string_view spread_profile;
    double bid_price{};
    double ask_price{};
    float bid_spread{};
    float ask_spread{};
};

std::expected<std::vector<std::byte>, EncodeError> encode_bbo_frame(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms);

std::expected<std::vector<BboRow>, DecodeError> flatten_bbo_frame(
    std::span<const std::byte> frame);

std::expected<std::vector<BboRowView>, DecodeError> flatten_bbo_frame_views(
    std::span<const std::byte> frame);

} // namespace swissquote::codec
