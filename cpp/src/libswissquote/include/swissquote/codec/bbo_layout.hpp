#pragma once

#include <cstdint>
#include <string_view>

namespace swissquote::codec {

inline constexpr std::uint16_t kRootBlockLength = 12;
inline constexpr std::uint16_t kVenueBlockLength = 16;
inline constexpr std::uint16_t kProfileBlockLength = 28;
inline constexpr std::uint16_t kBboTemplateId = 1;
inline constexpr std::uint16_t kBboSchemaId = 1;
inline constexpr std::uint16_t kBboSchemaVersion = 1;
inline constexpr std::uint16_t kSbeHeaderSize = 8;

} // namespace swissquote::codec
