#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace swissquote::test {

inline constexpr std::string_view kSampleSymbol = "EUR/USD";
inline constexpr std::uint64_t kSampleFetchTimeMs = 1'700'000'000'000ULL;
inline constexpr std::size_t kSampleFrameLen = 448;

inline constexpr std::string_view kSampleJson =
    R"([{"topo":{"platform":"SwissquoteCapitalMarkets","server":"Live7"},)"
    R"("spreadProfilePrices":[{"spreadProfile":"premium","bidSpread":0.65,)"
    R"("askSpread":0.65,"bid":1.13768,"ask":1.13798},{"spreadProfile":"prime",)"
    R"("bidSpread":0.4,"askSpread":0.4,"bid":1.1377,"ask":1.13795},)"
    R"({"spreadProfile":"elite","bidSpread":0.0,"askSpread":0.0,"bid":1.13774,)"
    R"("ask":1.13791}],"ts":1782941100045},{"topo":{"platform":"AT","server":"AT"},)"
    R"("spreadProfilePrices":[{"spreadProfile":"standard","bidSpread":0.85,)"
    R"("askSpread":0.85,"bid":1.13766,"ask":1.138},{"spreadProfile":"premium",)"
    R"("bidSpread":0.7,"askSpread":0.7,"bid":1.13767,"ask":1.13798},)"
    R"({"spreadProfile":"prime","bidSpread":0.55,"askSpread":0.55,"bid":1.13769,)"
    R"("ask":1.13797}],"ts":1782941100045},{"topo":{"platform":"SwissquoteLtd",)"
    R"("server":"Live5"},"spreadProfilePrices":[{"spreadProfile":"premium",)"
    R"("bidSpread":0.65,"askSpread":0.65,"bid":1.13768,"ask":1.13798},)"
    R"({"spreadProfile":"prime","bidSpread":0.4,"askSpread":0.4,"bid":1.1377,)"
    R"("ask":1.13795},{"spreadProfile":"elite","bidSpread":0.0,"askSpread":0.0,)"
    R"("bid":1.13774,"ask":1.13791}],"ts":1782941100045}])";

} // namespace swissquote::test
