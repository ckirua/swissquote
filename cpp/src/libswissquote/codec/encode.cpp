#define SIMDJSON_EXCEPTIONS 0

#include "swissquote/codec/codec.hpp"
#include "swissquote/codec/bbo_layout.hpp"

#include "simdjson.h"

#include <cstring>
#include <string>
#include <vector>

namespace swissquote::codec {
namespace {

using namespace simdjson;
using namespace simdjson::ondemand;

inline void write_u16(char*& p, std::uint16_t v) {
    std::memcpy(p, &v, 2);
    p += 2;
}

inline void write_u32(char*& p, std::uint32_t v) {
    std::memcpy(p, &v, 4);
    p += 4;
}

inline void write_u64(char*& p, std::uint64_t v) {
    std::memcpy(p, &v, 8);
    p += 8;
}

inline void write_f32(char*& p, float v) {
    std::memcpy(p, &v, 4);
    p += 4;
}

inline void write_f64(char*& p, double v) {
    std::memcpy(p, &v, 8);
    p += 8;
}

inline void write_bytes(char*& p, const char* data, std::size_t len) {
    std::memcpy(p, data, len);
    p += len;
}

struct ProfileFields {
    std::string spread_profile;
    double bid{};
    double ask{};
    float bid_spread{};
    float ask_spread{};
};

struct VenueFields {
    std::string platform;
    std::string server;
    std::uint64_t venue_ts{};
    std::vector<ProfileFields> profiles;
};

std::expected<void, EncodeError> read_string_field(ondemand::object obj, const char* key, std::string& out) {
    std::string_view value;
    auto error = obj[key].get_string().get(value);
    if (error == NO_SUCH_FIELD) {
        out.clear();
        return {};
    }
    if (error) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    out.assign(value);
    return {};
}

std::expected<void, EncodeError> read_profile(ondemand::object profile_obj, ProfileFields& out) {
    if (auto rc = read_string_field(profile_obj, "spreadProfile", out.spread_profile); !rc) {
        return rc;
    }

    double bid = 0.0;
    double ask = 0.0;
    double bid_spread = 0.0;
    double ask_spread = 0.0;
    auto error = profile_obj["bid"].get_double().get(bid);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    error = profile_obj["ask"].get_double().get(ask);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    error = profile_obj["bidSpread"].get_double().get(bid_spread);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }
    error = profile_obj["askSpread"].get_double().get(ask_spread);
    if (error && error != NO_SUCH_FIELD) {
        return std::unexpected(EncodeError::InvalidJson);
    }

    out.bid = bid;
    out.ask = ask;
    out.bid_spread = static_cast<float>(bid_spread);
    out.ask_spread = static_cast<float>(ask_spread);
    return {};
}

std::expected<void, EncodeError> read_profiles(ondemand::object venue_obj, std::vector<ProfileFields>& profiles_out) {
    profiles_out.clear();
    profiles_out.reserve(8);
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
        ProfileFields fields;
        if (auto rc = read_profile(profile_obj, fields); !rc) {
            return rc;
        }
        profiles_out.push_back(std::move(fields));
    }
    return {};
}

std::expected<void, EncodeError> read_venue(ondemand::object venue_obj, VenueFields& out) {
    ondemand::object topo;
    auto error = venue_obj["topo"].get_object().get(topo);
    if (error) {
        return std::unexpected(EncodeError::VenueMissingTopo);
    }

    if (auto rc = read_string_field(topo, "platform", out.platform); !rc) {
        return rc;
    }
    if (auto rc = read_string_field(topo, "server", out.server); !rc) {
        return rc;
    }

    std::int64_t ts = 0;
    error = venue_obj["ts"].get_int64().get(ts);
    out.venue_ts = error ? 0ULL : static_cast<std::uint64_t>(ts);

    return read_profiles(venue_obj, out.profiles);
}

std::size_t profile_wire_size(const ProfileFields& profile) {
    return kProfileBlockLength + profile.spread_profile.size();
}

std::size_t venue_wire_size(const VenueFields& venue) {
    std::size_t total = kVenueBlockLength + venue.platform.size() + venue.server.size() + 4;
    for (const auto& profile : venue.profiles) {
        total += profile_wire_size(profile);
    }
    return total;
}

std::size_t compute_wire_size(std::size_t symbol_len, const std::vector<VenueFields>& venues) {
    std::size_t total = 8 + kRootBlockLength + symbol_len + 4;
    for (const auto& venue : venues) {
        total += venue_wire_size(venue);
    }
    return total;
}

void write_profile(char*& p, const ProfileFields& profile) {
    write_u32(p, static_cast<std::uint32_t>(profile.spread_profile.size()));
    write_f64(p, profile.bid);
    write_f64(p, profile.ask);
    write_f32(p, profile.bid_spread);
    write_f32(p, profile.ask_spread);
    write_bytes(p, profile.spread_profile.data(), profile.spread_profile.size());
}

void write_venue(char*& p, const VenueFields& venue) {
    write_u32(p, static_cast<std::uint32_t>(venue.platform.size()));
    write_u32(p, static_cast<std::uint32_t>(venue.server.size()));
    write_u64(p, venue.venue_ts);
    write_bytes(p, venue.platform.data(), venue.platform.size());
    write_bytes(p, venue.server.data(), venue.server.size());

    write_u16(p, kProfileBlockLength);
    write_u16(p, static_cast<std::uint16_t>(venue.profiles.size()));
    for (const auto& profile : venue.profiles) {
        write_profile(p, profile);
    }
}

void write_frame(
    char* buf,
    std::size_t symbol_len,
    const char* symbol,
    std::uint64_t fetch_time_ms,
    const std::vector<VenueFields>& venues) {
    char* p = buf;
    write_u16(p, kRootBlockLength);
    write_u16(p, kBboTemplateId);
    write_u16(p, kBboSchemaId);
    write_u16(p, kBboSchemaVersion);

    write_u32(p, static_cast<std::uint32_t>(symbol_len));
    write_u64(p, fetch_time_ms);
    write_bytes(p, symbol, symbol_len);

    write_u16(p, kVenueBlockLength);
    write_u16(p, static_cast<std::uint16_t>(venues.size()));
    for (const auto& venue : venues) {
        write_venue(p, venue);
    }
}

std::expected<std::vector<VenueFields>, EncodeError> parse_venues(const char* json, std::size_t json_len) {
    ondemand::parser parser;
    padded_string padded(json, json_len);

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

    std::vector<VenueFields> venues_out;
    venues_out.reserve(16);
    for (auto venue : venues) {
        ondemand::object venue_obj;
        error = venue.get_object().get(venue_obj);
        if (error) {
            return std::unexpected(EncodeError::VenueNotObject);
        }
        VenueFields fields;
        if (auto rc = read_venue(venue_obj, fields); !rc) {
            return std::unexpected(rc.error());
        }
        venues_out.push_back(std::move(fields));
    }
    return venues_out;
}

} // namespace

std::expected<std::vector<std::byte>, EncodeError> encode_bbo_frame(
    std::string_view symbol,
    std::span<const std::byte> json,
    std::uint64_t fetch_time_ms) {
    const auto* json_ptr = reinterpret_cast<const char*>(json.data());
    auto venues = parse_venues(json_ptr, json.size());
    if (!venues) {
        return std::unexpected(venues.error());
    }

    const std::size_t frame_size = compute_wire_size(symbol.size(), *venues);
    std::vector<std::byte> buf(frame_size);
    write_frame(
        reinterpret_cast<char*>(buf.data()),
        symbol.size(),
        symbol.data(),
        fetch_time_ms,
        *venues);
    return buf;
}

} // namespace swissquote::codec
