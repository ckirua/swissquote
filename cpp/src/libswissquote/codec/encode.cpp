#define SIMDJSON_EXCEPTIONS 0

#include "swissquote/codec/codec.hpp"

#include "simdjson.h"
#include "swissquote_bbo/BboSnapshot.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace swissquote::codec {

struct EncodeWorkspace::Impl {
    simdjson::ondemand::parser parser;
    std::vector<char> padded;
};

EncodeWorkspace::EncodeWorkspace() : impl_(std::make_unique<Impl>()) {}
EncodeWorkspace::~EncodeWorkspace() = default;
EncodeWorkspace::EncodeWorkspace(EncodeWorkspace&&) noexcept = default;
EncodeWorkspace& EncodeWorkspace::operator=(EncodeWorkspace&&) noexcept = default;

namespace {

using BboSnapshot = swissquote::bbo::BboSnapshot;
using MessageHeader = swissquote::bbo::MessageHeader;
using namespace simdjson;
using namespace simdjson::ondemand;

constexpr std::uint16_t kMaxGroupCount = 65534;

bool valid_var_length(std::string_view value) {
    return value.size() <= std::numeric_limits<std::uint32_t>::max()
        && value.size() <= 65536;
}

bool valid_ascii(std::string_view value) {
    return std::all_of(
        value.begin(), value.end(), [](unsigned char character) { return character <= 0x7f; });
}

std::expected<void, EncodeError> read_string_field(
    ondemand::object obj,
    const char* key,
    std::string_view& out) {
    auto error = obj[key].get_string().get(out);
    if (error == NO_SUCH_FIELD) {
        out = {};
        return {};
    }
    if (error) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    return {};
}

std::expected<void, EncodeError> read_profile(
    ondemand::object profile_obj,
    std::string_view& spread_profile,
    double& bid,
    double& ask,
    float& bid_spread,
    float& ask_spread) {
    if (auto rc = read_string_field(profile_obj, "spreadProfile", spread_profile); !rc) {
        return rc;
    }

    double bid_value = 0.0;
    double ask_value = 0.0;
    double bid_spread_value = 0.0;
    double ask_spread_value = 0.0;
    auto error = profile_obj["bid"].get_double().get(bid_value);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    error = profile_obj["ask"].get_double().get(ask_value);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    error = profile_obj["bidSpread"].get_double().get(bid_spread_value);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    error = profile_obj["askSpread"].get_double().get(ask_spread_value);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }

    bid = bid_value;
    ask = ask_value;
    bid_spread = static_cast<float>(bid_spread_value);
    ask_spread = static_cast<float>(ask_spread_value);
    return {};
}

template<typename ProfileFn>
std::expected<void, EncodeError> read_profiles(
    ondemand::object venue_obj,
    std::size_t& profile_count,
    ProfileFn&& profile_fn) {
    profile_count = 0;
    ondemand::array profiles_array;
    auto error = venue_obj["spreadProfilePrices"].get_array().get(profiles_array);
    if (error == NO_SUCH_FIELD) {
        return {};
    }
    if (error) {
        return std::unexpected(EncodeError::ProfilesNotArray);
    }

    for (auto profile : profiles_array) {
        ondemand::object profile_obj;
        error = profile.get_object().get(profile_obj);
        if (error) {
            return std::unexpected(EncodeError::ProfileNotObject);
        }

        std::string_view spread_profile;
        double bid = 0.0;
        double ask = 0.0;
        float bid_spread = 0.0F;
        float ask_spread = 0.0F;
        if (auto rc = read_profile(
                profile_obj, spread_profile, bid, ask, bid_spread, ask_spread);
            !rc) {
            return rc;
        }
        if (auto rc = profile_fn(spread_profile, bid, ask, bid_spread, ask_spread); !rc) {
            return rc;
        }
        ++profile_count;
    }
    return {};
}

std::expected<void, EncodeError> read_venue_header(
    ondemand::object venue_obj,
    std::string_view& platform,
    std::string_view& server,
    std::uint64_t& venue_ts) {
    ondemand::object topo;
    auto error = venue_obj["topo"].get_object().get(topo);
    if (error) {
        return std::unexpected(EncodeError::VenueMissingTopo);
    }

    if (auto rc = read_string_field(topo, "platform", platform); !rc) {
        return rc;
    }
    if (auto rc = read_string_field(topo, "server", server); !rc) {
        return rc;
    }

    std::int64_t ts = 0;
    error = venue_obj["ts"].get_int64().get(ts);
    venue_ts = error ? 0ULL : static_cast<std::uint64_t>(ts);
    return {};
}

template<typename VenueFn>
std::expected<void, EncodeError> parse_document(
    ondemand::parser& parser,
    const padded_string_view& padded,
    VenueFn&& venue_fn) {
    ondemand::document doc;
    auto error = parser.iterate(padded).get(doc);
    if (error) {
        return std::unexpected(EncodeError::InvalidJson);
    }

    ondemand::array venues;
    error = doc.get_array().get(venues);
    if (error) {
        return std::unexpected(EncodeError::NotArray);
    }

    for (auto venue : venues) {
        ondemand::object venue_obj;
        error = venue.get_object().get(venue_obj);
        if (error) {
            return std::unexpected(EncodeError::VenueNotObject);
        }
        if (auto rc = venue_fn(venue_obj); !rc) {
            return rc;
        }
    }
    return {};
}

struct FrameLayout {
    std::size_t size;
    std::size_t venue_count;
    bool invalid_limits;
};

std::expected<FrameLayout, EncodeError> measure_frame(
    std::string_view symbol,
    ondemand::parser& parser,
    const padded_string_view& padded) {
    FrameLayout layout{
        MessageHeader::encodedLength() + BboSnapshot::sbeBlockLength()
            + symbol.size() + BboSnapshot::Venues::sbeHeaderSize(),
        0,
        false};

    auto parsed = parse_document(
        parser,
        padded,
        [&](ondemand::object venue_obj) -> std::expected<void, EncodeError> {
            ++layout.venue_count;
            std::string_view platform;
            std::string_view server;
            [[maybe_unused]] std::uint64_t venue_ts = 0;
            if (auto rc = read_venue_header(venue_obj, platform, server, venue_ts); !rc) {
                return rc;
            }

            std::size_t profile_count = 0;
            if (auto rc = read_profiles(
                    venue_obj,
                    profile_count,
                    [&](std::string_view spread_profile,
                        double,
                        double,
                        float,
                        float) -> std::expected<void, EncodeError> {
                        layout.size += BboSnapshot::Venues::Profiles::sbeBlockLength()
                            + spread_profile.size();
                        if (!valid_var_length(spread_profile) || !valid_ascii(spread_profile)) {
                            layout.invalid_limits = true;
                        }
                        return {};
                    });
                !rc) {
                return rc;
            }

            layout.size += BboSnapshot::Venues::sbeBlockLength()
                + platform.size() + server.size()
                + BboSnapshot::Venues::Profiles::sbeHeaderSize();
            if (!valid_var_length(platform) || !valid_var_length(server)
                || !valid_ascii(platform) || !valid_ascii(server)
                || profile_count > kMaxGroupCount) {
                layout.invalid_limits = true;
            }
            return {};
        });
    if (!parsed) {
        return std::unexpected(parsed.error());
    }

    // The old implementation parsed all JSON before applying any size checks.
    // Keep that ordering so malformed input retains its original error mapping.
    if (!valid_var_length(symbol) || !valid_ascii(symbol)) {
        layout.invalid_limits = true;
    }
    if (layout.venue_count > kMaxGroupCount) {
        layout.invalid_limits = true;
    }
    return layout;
}

std::expected<void, EncodeError> write_frame(
    char* buffer,
    std::size_t buffer_length,
    std::string_view symbol,
    std::uint64_t fetch_time_ms,
    ondemand::parser& parser,
    const padded_string_view& padded) {
    try {
        BboSnapshot snapshot;
        snapshot.wrapAndApplyHeader(buffer, 0, buffer_length).fetchTimeMs(fetch_time_ms);

        auto& symbol_field = snapshot.symbol();
        auto position = snapshot.sbePosition();
        symbol_field.put(symbol, position);
        snapshot.sbePosition(position);

        auto& venue_group = snapshot.venuesCount(kMaxGroupCount);
        auto parsed = parse_document(
            parser,
            padded,
            [&](ondemand::object venue_obj) -> std::expected<void, EncodeError> {
                std::string_view platform;
                std::string_view server;
                std::uint64_t venue_ts = 0;
                if (auto rc = read_venue_header(venue_obj, platform, server, venue_ts); !rc) {
                    return rc;
                }

                auto& venue = venue_group.next();
                venue.venueTsMs(venue_ts);
                auto venue_position = venue.sbePosition();
                venue.platform().put(platform, venue_position);
                venue.server().put(server, venue_position);
                venue.sbePosition(venue_position);

                auto& profile_group = venue.profilesCount(kMaxGroupCount);
                [[maybe_unused]] std::size_t profile_count = 0;
                if (auto rc = read_profiles(
                        venue_obj,
                        profile_count,
                        [&](std::string_view spread_profile,
                            double bid,
                            double ask,
                            float bid_spread,
                            float ask_spread) -> std::expected<void, EncodeError> {
                            profile_group.next()
                                .bid(bid)
                                .ask(ask)
                                .bidSpread(bid_spread)
                                .askSpread(ask_spread);
                            auto profile_position = profile_group.sbePosition();
                            profile_group.spreadProfile().put(spread_profile, profile_position);
                            profile_group.sbePosition(profile_position);
                            return {};
                        });
                    !rc) {
                    return rc;
                }
                profile_group.resetCountToIndex();
                return {};
            });
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        venue_group.resetCountToIndex();
        if (snapshot.encodedLength() != buffer_length - MessageHeader::encodedLength()) {
            return std::unexpected(EncodeError::InvalidJson);
        }
        return {};
    } catch (const std::exception&) {
        return std::unexpected(EncodeError::InvalidJson);
    }
}

} // namespace

std::expected<void, EncodeError> encode_bbo_frame_into(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms,
    EncodeWorkspace& workspace) {
    workspace.output.clear();
    auto& padded = workspace.impl_->padded;
    padded.resize(json.size() + SIMDJSON_PADDING);
    std::memcpy(padded.data(), json.data(), json.size());
    std::memset(padded.data() + json.size(), 0, SIMDJSON_PADDING);
    const padded_string_view padded_view(padded.data(), json.size(), padded.size());
    auto& parser = workspace.impl_->parser;

    auto layout = measure_frame(symbol, parser, padded_view);
    if (!layout) {
        return std::unexpected(layout.error());
    }
    if (layout->invalid_limits) {
        return std::unexpected(EncodeError::InvalidJson);
    }

    workspace.output.resize(layout->size);
    if (auto rc = write_frame(
            reinterpret_cast<char*>(workspace.output.data()),
            workspace.output.size(),
            symbol,
            fetch_time_ms,
            parser,
            padded_view);
        !rc) {
        workspace.output.clear();
        return rc;
    }
    return {};
}

std::expected<void, EncodeError> encode_bbo_frame_into(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms,
    std::vector<std::byte>& output) {
    EncodeWorkspace workspace;
    if (auto rc = encode_bbo_frame_into(symbol, json, fetch_time_ms, workspace); !rc) {
        return rc;
    }
    output = std::move(workspace.output);
    return {};
}

std::expected<std::vector<std::byte>, EncodeError> encode_bbo_frame(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms) {
    std::vector<std::byte> buffer;
    if (auto rc = encode_bbo_frame_into(symbol, json, fetch_time_ms, buffer); !rc) {
        return std::unexpected(rc.error());
    }
    return buffer;
}

} // namespace swissquote::codec
