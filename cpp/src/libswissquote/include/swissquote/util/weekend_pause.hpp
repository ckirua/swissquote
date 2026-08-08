#pragma once

#include <chrono>

namespace swissquote::util {

/// True when `tp` falls in the FX weekend pause window:
/// [Fri 16:55, Sun 16:50) America/New_York (DST-safe via IANA tzdb).
/// Hosts need zoneinfo (`tzdata`); standard on Linux deploy images.
[[nodiscard]] bool in_weekend_pause(std::chrono::system_clock::time_point tp);

} // namespace swissquote::util
