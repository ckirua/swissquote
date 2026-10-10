#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
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

class EncodeWorkspace {
public:
    EncodeWorkspace();
    ~EncodeWorkspace();
    EncodeWorkspace(EncodeWorkspace&&) noexcept;
    EncodeWorkspace& operator=(EncodeWorkspace&&) noexcept;
    EncodeWorkspace(const EncodeWorkspace&) = delete;
    EncodeWorkspace& operator=(const EncodeWorkspace&) = delete;

    std::vector<std::byte> output;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend std::expected<void, EncodeError> encode_bbo_frame_into(
        std::string_view,
        std::span<const std::byte>,
        std::uint64_t,
        EncodeWorkspace&);
};

std::expected<std::vector<std::byte>, EncodeError> encode_bbo_frame(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms);
std::expected<void, EncodeError> encode_bbo_frame_into(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms,
    EncodeWorkspace& workspace);

std::expected<void, EncodeError> encode_bbo_frame_into(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms,
    std::vector<std::byte>& output);

std::expected<std::vector<BboRow>, DecodeError> flatten_bbo_frame(
    std::span<const std::byte> frame);
std::expected<void, DecodeError> flatten_bbo_frame(
    std::span<const std::byte> frame,
    std::vector<BboRow>& output);

std::expected<void, DecodeError> flatten_bbo_frame_views(
    std::span<const std::byte> frame,
    std::vector<BboRowView>& output);

std::expected<std::vector<BboRowView>, DecodeError> flatten_bbo_frame_views(
    std::span<const std::byte> frame);

} // namespace swissquote::codec
