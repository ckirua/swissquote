#include <swissquote/util/weekend_pause.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using swissquote::util::in_weekend_pause;

namespace {

using namespace std::chrono;

/// Build a system_clock time_point from a UTC civil datetime.
[[nodiscard]] system_clock::time_point utc(
    int y, month mon, int d, int hh, int mm, int ss = 0) {
    const sys_days day = sys_days{year{y} / mon / d};
    return system_clock::time_point{day + hours{hh} + minutes{mm} + seconds{ss}};
}

} // namespace

TEST_CASE("weekend pause EDT summer edges", "[weekend_pause]") {
    // 2026-07-24 is Friday; EDT = UTC-4.
    // Fri 16:54 NY = 20:54 UTC → outside pause
    CHECK_FALSE(in_weekend_pause(utc(2026, July, 24, 20, 54)));
    // Fri 16:55 NY = 20:55 UTC → pause starts
    CHECK(in_weekend_pause(utc(2026, July, 24, 20, 55)));
    // Sat noon NY = 16:00 UTC
    CHECK(in_weekend_pause(utc(2026, July, 25, 16, 0)));
    // Sun 16:49 NY = 20:49 UTC → still paused
    CHECK(in_weekend_pause(utc(2026, July, 26, 20, 49)));
    // Sun 16:50 NY = 20:50 UTC → resume
    CHECK_FALSE(in_weekend_pause(utc(2026, July, 26, 20, 50)));
}

TEST_CASE("weekend pause EST winter edges", "[weekend_pause]") {
    // 2026-01-09 is Friday; EST = UTC-5.
    // Fri 16:54 NY = 21:54 UTC → outside pause
    CHECK_FALSE(in_weekend_pause(utc(2026, January, 9, 21, 54)));
    // Fri 16:55 NY = 21:55 UTC → pause starts
    CHECK(in_weekend_pause(utc(2026, January, 9, 21, 55)));
    // Sat noon NY = 17:00 UTC
    CHECK(in_weekend_pause(utc(2026, January, 10, 17, 0)));
    // Sun 16:49 NY = 21:49 UTC → still paused
    CHECK(in_weekend_pause(utc(2026, January, 11, 21, 49)));
    // Sun 16:50 NY = 21:50 UTC → resume
    CHECK_FALSE(in_weekend_pause(utc(2026, January, 11, 21, 50)));
}

TEST_CASE("weekend pause midweek never active", "[weekend_pause]") {
    // Wednesday 2026-07-22 noon EDT = 16:00 UTC
    CHECK_FALSE(in_weekend_pause(utc(2026, July, 22, 16, 0)));
}
