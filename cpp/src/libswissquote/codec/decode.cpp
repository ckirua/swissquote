#include "swissquote/codec/codec.hpp"
#include "swissquote/codec/bbo_layout.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace swissquote::codec {
namespace {

inline std::uint16_t read_u16_le(const char* p) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(p[0]))
        | (static_cast<std::uint16_t>(static_cast<unsigned char>(p[1])) << 8);
}

inline std::uint32_t read_u32_le(const char* p) noexcept {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(p[0]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(p[1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(p[2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(p[3])) << 24);
}

inline std::uint64_t read_u64_le(const char* p) noexcept {
    const std::uint64_t lo = read_u32_le(p);
    return lo | (static_cast<std::uint64_t>(read_u32_le(p + 4)) << 32);
}

inline float read_f32_le(const char* p) noexcept {
    std::uint32_t bits = read_u32_le(p);
    float out;
    std::memcpy(&out, &bits, sizeof(float));
    return out;
}

inline double read_f64_le(const char* p) noexcept {
    std::uint64_t bits = read_u64_le(p);
    double out;
    std::memcpy(&out, &bits, sizeof(double));
    return out;
}

bool is_valid_ascii(const char* p, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        if (static_cast<unsigned char>(p[i]) > 127) {
            return false;
        }
    }
    return true;
}

std::string decode_ascii(const char* p, std::size_t n) {
    return std::string(p, n);
}

void bounds_check(std::ptrdiff_t offset, std::ptrdiff_t need, std::ptrdiff_t frame_len) {
    if (offset < 0 || offset + need > frame_len) {
        throw std::runtime_error("frame too short");
    }
}

std::ptrdiff_t validate_header(const char* buf, std::ptrdiff_t frame_len) {
    bounds_check(0, kSbeHeaderSize + kRootBlockLength, frame_len);

    const std::uint16_t block_length = read_u16_le(buf);
    const std::uint16_t template_id = read_u16_le(buf + 2);
    const std::uint16_t schema_id = read_u16_le(buf + 4);
    const std::uint16_t version = read_u16_le(buf + 6);

    if (template_id != kBboTemplateId || schema_id != kBboSchemaId || version != kBboSchemaVersion
        || block_length != kRootBlockLength) {
        throw std::runtime_error("invalid sbe header");
    }
    return kSbeHeaderSize;
}

} // namespace

std::expected<std::vector<BboRowView>, DecodeError> flatten_bbo_frame_views(
    std::span<const std::byte> frame) {
    const char* buf = reinterpret_cast<const char*>(frame.data());
    const std::ptrdiff_t frame_len = static_cast<std::ptrdiff_t>(frame.size());
    std::vector<BboRowView> rows;
    rows.reserve(16);

    try {
        std::ptrdiff_t offset = validate_header(buf, frame_len);

        const std::ptrdiff_t sym_len = static_cast<std::ptrdiff_t>(read_u32_le(buf + offset));
        offset += 4;
        offset += 8; // fetch_time_ms unused in flattened rows
        bounds_check(offset, sym_len, frame_len);
        if (!is_valid_ascii(buf + offset, static_cast<std::size_t>(sym_len))) {
            return std::unexpected(DecodeError::InvalidAscii);
        }
        const std::string_view symbol{buf + offset, static_cast<std::size_t>(sym_len)};
        offset += sym_len;

        const std::uint16_t venue_block_len = read_u16_le(buf + offset);
        const int venue_count = static_cast<int>(read_u16_le(buf + offset + 2));
        offset += 4;
        if (venue_block_len != kVenueBlockLength) {
            return std::unexpected(DecodeError::InvalidHeader);
        }

        for (int v = 0; v < venue_count; ++v) {
            bounds_check(offset, kVenueBlockLength, frame_len);
            const std::ptrdiff_t plat_len = static_cast<std::ptrdiff_t>(read_u32_le(buf + offset));
            const std::ptrdiff_t srv_len = static_cast<std::ptrdiff_t>(read_u32_le(buf + offset + 4));
            const std::uint64_t venue_ts_ms = read_u64_le(buf + offset + 8);
            offset += kVenueBlockLength;

            bounds_check(offset, plat_len + srv_len, frame_len);
            if (!is_valid_ascii(buf + offset, static_cast<std::size_t>(plat_len))
                || !is_valid_ascii(buf + offset + plat_len, static_cast<std::size_t>(srv_len))) {
                return std::unexpected(DecodeError::InvalidAscii);
            }
            const std::string_view platform{buf + offset, static_cast<std::size_t>(plat_len)};
            offset += plat_len;
            const std::string_view server{buf + offset, static_cast<std::size_t>(srv_len)};
            offset += srv_len;

            bounds_check(offset, 4, frame_len);
            const std::uint16_t prof_block_len = read_u16_le(buf + offset);
            const int prof_count = static_cast<int>(read_u16_le(buf + offset + 2));
            offset += 4;
            if (prof_block_len != kProfileBlockLength) {
                return std::unexpected(DecodeError::InvalidHeader);
            }

            for (int p = 0; p < prof_count; ++p) {
                bounds_check(offset, kProfileBlockLength, frame_len);
                const std::ptrdiff_t prof_len = static_cast<std::ptrdiff_t>(read_u32_le(buf + offset));
                const double bid = read_f64_le(buf + offset + 4);
                const double ask = read_f64_le(buf + offset + 12);
                const float bid_spread = read_f32_le(buf + offset + 20);
                const float ask_spread = read_f32_le(buf + offset + 24);
                offset += kProfileBlockLength;
                bounds_check(offset, prof_len, frame_len);
                if (!is_valid_ascii(buf + offset, static_cast<std::size_t>(prof_len))) {
                    return std::unexpected(DecodeError::InvalidAscii);
                }
                const std::string_view spread_profile{
                    buf + offset, static_cast<std::size_t>(prof_len)};
                offset += prof_len;

                rows.push_back(BboRowView{
                    .ts_ms = static_cast<std::int64_t>(venue_ts_ms),
                    .symbol = symbol,
                    .platform = platform,
                    .server = server,
                    .spread_profile = spread_profile,
                    .bid_price = bid,
                    .ask_price = ask,
                    .bid_spread = bid_spread,
                    .ask_spread = ask_spread,
                });

            }
        }
    } catch (const std::runtime_error&) {
        return std::unexpected(DecodeError::FrameTooShort);
    }

    return rows;
}
std::expected<std::vector<BboRow>, DecodeError> flatten_bbo_frame(
    std::span<const std::byte> frame) {
    auto views = flatten_bbo_frame_views(frame);
    if (!views) {
        return std::unexpected(views.error());
    }

    std::vector<BboRow> rows;
    rows.reserve(views->size());
    for (const auto& row : *views) {
        rows.push_back(BboRow{
            .ts_ms = row.ts_ms,
            .symbol = std::string(row.symbol),
            .platform = std::string(row.platform),
            .server = std::string(row.server),
            .spread_profile = std::string(row.spread_profile),
            .bid_price = row.bid_price,
            .ask_price = row.ask_price,
            .bid_spread = row.bid_spread,
            .ask_spread = row.ask_spread,
        });
    }
    return rows;
}

} // namespace swissquote::codec
