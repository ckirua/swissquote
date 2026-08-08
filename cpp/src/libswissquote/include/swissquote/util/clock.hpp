#pragma once

#include <chrono>

namespace swissquote::util {

[[nodiscard]] std::int64_t clock_realtime_ms() noexcept;
[[nodiscard]] double monotonic_sec() noexcept;

} // namespace swissquote::util
