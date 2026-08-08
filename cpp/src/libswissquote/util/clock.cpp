#include "swissquote/util/clock.hpp"

#include <ctime>

namespace swissquote::util {

std::int64_t clock_realtime_ms() noexcept {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

double monotonic_sec() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

} // namespace swissquote::util
