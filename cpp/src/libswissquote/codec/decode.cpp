#include "swissquote/codec/codec.hpp"

#include "swissquote_bbo/BboSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace swissquote::codec {
namespace {

using BboSnapshot = swissquote::bbo::BboSnapshot;
using MessageHeader = swissquote::bbo::MessageHeader;

template <typename Emit>
std::expected<void, DecodeError> decode_rows(
    std::span<const std::byte> frame,
    Emit&& emit) {
    if (frame.size() < MessageHeader::encodedLength()) {
        return std::unexpected(DecodeError::FrameTooShort);
    }

    char* const buffer = const_cast<char*>(reinterpret_cast<const char*>(frame.data()));
    const auto buffer_length = static_cast<std::uint64_t>(frame.size());

    try {
        const MessageHeader header(buffer, buffer_length, BboSnapshot::sbeSchemaVersion());
        if (header.blockLength() != BboSnapshot::sbeBlockLength()
            || header.templateId() != BboSnapshot::sbeTemplateId()
            || header.schemaId() != BboSnapshot::sbeSchemaId()
            || header.version() != BboSnapshot::sbeSchemaVersion()) {
            return std::unexpected(DecodeError::InvalidHeader);
        }

        BboSnapshot snapshot;
        snapshot.wrapForDecode(
            buffer,
            MessageHeader::encodedLength(),
            header.blockLength(),
            header.version(),
            buffer_length);

        auto& symbol_field = snapshot.symbol();
        auto position = snapshot.sbePosition();
        const auto symbol = symbol_field.get(position);
        snapshot.sbePosition(position);

        auto& venue_group = snapshot.venues();
        if (venue_group.sbeActingBlockLength() != BboSnapshot::Venues::sbeBlockLength()) {
            return std::unexpected(DecodeError::InvalidHeader);
        }
        while (venue_group.hasNext()) {
            auto& venue = venue_group.next();
            position = venue_group.sbePosition();
            const auto platform = venue.platform().get(position);
            const auto server = venue.server().get(position);
            venue_group.sbePosition(position);

            auto& profile_group = venue.profiles();
            if (profile_group.sbeActingBlockLength()
                != BboSnapshot::Venues::Profiles::sbeBlockLength()) {
                return std::unexpected(DecodeError::InvalidHeader);
            }
            while (profile_group.hasNext()) {
                auto& profile = profile_group.next();
                position = profile_group.sbePosition();
                const auto spread_profile = profile.spreadProfile().get(position);
                profile_group.sbePosition(position);

                emit(BboRowView{
                    .ts_ms = static_cast<std::int64_t>(venue.venueTsMs()),
                    .symbol = symbol,
                    .platform = platform,
                    .server = server,
                    .spread_profile = spread_profile,
                    .bid_price = profile.bid(),
                    .ask_price = profile.ask(),
                    .bid_spread = profile.bidSpread(),
                    .ask_spread = profile.askSpread(),
                });
            }
        }
    } catch (const std::runtime_error& error) {
        const std::string_view message(error.what());
        return std::unexpected(
            message.find("[E111]") != std::string_view::npos
                ? DecodeError::InvalidAscii
                : DecodeError::FrameTooShort);
    } catch (const std::exception&) {
        return std::unexpected(DecodeError::FrameTooShort);
    }
    return {};
}

std::expected<void, DecodeError> decode_views(
    std::span<const std::byte> frame,
    std::vector<BboRowView>& rows) {
    rows.clear();
    rows.reserve(rows.capacity() < 16 ? 16 : rows.capacity());
    auto result = decode_rows(frame, [&rows](BboRowView row) { rows.push_back(row); });
    if (!result) {
        rows.clear();
    }
    return result;
}

std::expected<void, DecodeError> decode_owned(
    std::span<const std::byte> frame,
    std::vector<BboRow>& rows) {
    rows.clear();
    rows.reserve(rows.capacity() < 16 ? 16 : rows.capacity());
    auto result = decode_rows(frame, [&rows](const BboRowView& row) {
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
    });
    if (!result) {
        rows.clear();
    }
    return result;
}


} // namespace

std::expected<std::vector<BboRowView>, DecodeError> flatten_bbo_frame_views(
    std::span<const std::byte> frame) {
    std::vector<BboRowView> rows;
    auto result = decode_views(frame, rows);
    if (!result) {
        return std::unexpected(result.error());
    }
    return rows;
}

std::expected<void, DecodeError> flatten_bbo_frame_views(
    std::span<const std::byte> frame,
    std::vector<BboRowView>& output) {
    return decode_views(frame, output);
}

std::expected<std::vector<BboRow>, DecodeError> flatten_bbo_frame(
    std::span<const std::byte> frame) {
    std::vector<BboRow> rows;
    auto result = decode_owned(frame, rows);
    if (!result) {
        return std::unexpected(result.error());
    }
    return rows;
}

std::expected<void, DecodeError> flatten_bbo_frame(
    std::span<const std::byte> frame,
    std::vector<BboRow>& output) {
    return decode_owned(frame, output);
}

} // namespace swissquote::codec
