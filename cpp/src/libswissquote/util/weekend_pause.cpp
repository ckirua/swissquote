#include "swissquote/util/weekend_pause.hpp"

namespace swissquote::util {

bool in_weekend_pause(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;

    const zoned_time zt{"America/New_York", floor<seconds>(tp)};
    const auto local = zt.get_local_time();
    const auto day = floor<days>(local);
    const weekday wd{day};
    const auto minutes_since_midnight = duration_cast<minutes>(local - day).count();

    constexpr int kFriStartMin = 16 * 60 + 55; // 16:55
    constexpr int kSunEndMin = 16 * 60 + 50;   // 16:50

    if (wd == Friday) {
        return minutes_since_midnight >= kFriStartMin;
    }
    if (wd == Saturday) {
        return true;
    }
    if (wd == Sunday) {
        return minutes_since_midnight < kSunEndMin;
    }
    return false;
}

} // namespace swissquote::util
