#include <swissquote/codec/codec.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

using swissquote::codec::BboRowView;
using swissquote::codec::EncodeWorkspace;
using swissquote::codec::encode_bbo_frame_into;
using swissquote::codec::flatten_bbo_frame_views;

constexpr std::string_view kSymbol = "EUR/USD";
constexpr std::uint64_t kFetchTimeMs = 1'700'000'000'000ULL;
constexpr std::string_view kJson =
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

volatile std::uint64_t g_checksum = 0;

std::span<const std::byte> as_bytes(std::string_view value) {
    return {reinterpret_cast<const std::byte*>(value.data()), value.size()};
}

std::uint64_t parse_count(const char* text, const char* option) {
    char* end = nullptr;
    const auto value = std::strtoull(text, &end, 10);
    if (end == text || *end != '\0' || value == 0 || value > std::numeric_limits<std::size_t>::max()) {
        std::cerr << option << " requires a positive integer\n";
        std::exit(EXIT_FAILURE);
    }
    return value;
}

std::uint64_t row_checksum(const BboRowView& row) {
    std::uint64_t checksum = static_cast<std::uint64_t>(row.ts_ms);
    checksum ^= static_cast<std::uint64_t>(row.symbol.size()) << 7;
    checksum ^= static_cast<std::uint64_t>(row.platform.size()) << 17;
    checksum ^= static_cast<std::uint64_t>(row.server.size()) << 29;
    checksum ^= static_cast<std::uint64_t>(row.spread_profile.size()) << 41;
    return checksum;
}

} // namespace

int main(int argc, char** argv) {
    std::size_t iterations = 100'000;
    std::size_t warmup = 2'000;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--iterations" && index + 1 < argc) {
            iterations = static_cast<std::size_t>(parse_count(argv[++index], "--iterations"));
        } else if (argument == "--warmup" && index + 1 < argc) {
            warmup = static_cast<std::size_t>(parse_count(argv[++index], "--warmup"));
        } else if (argument == "--help") {
            std::cout << "usage: swissquote_codec_benchmark [--iterations N] [--warmup N]\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "unknown argument: " << argument << '\n';
            return EXIT_FAILURE;
        }
    }

    EncodeWorkspace encode_workspace;
    if (!encode_bbo_frame_into(
            kSymbol, as_bytes(kJson), kFetchTimeMs, encode_workspace)) {
        std::cerr << "encode_bbo_frame_into failed for benchmark payload\n";
        return EXIT_FAILURE;
    }
    const std::span<const std::byte> frame = encode_workspace.output;
    std::vector<BboRowView> decoded_rows;

    const auto encode_once = [&] {
        if (!encode_bbo_frame_into(
                kSymbol, as_bytes(kJson), kFetchTimeMs, encode_workspace)) {
            return std::uint64_t{0};
        }
        std::uint64_t checksum = encode_workspace.output.size();
        for (const auto byte : encode_workspace.output) {
            checksum = (checksum * 33U) ^ std::to_integer<unsigned char>(byte);
        }
        return checksum;
    };
    const auto decode_once = [&] {
        const auto result = flatten_bbo_frame_views(frame, decoded_rows);
        if (!result) {
            return std::uint64_t{0};
        }
        std::uint64_t checksum = decoded_rows.size();
        for (const auto& row : decoded_rows) {
            checksum ^= row_checksum(row);
        }
        return checksum;
    };

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < warmup; ++index) {
        checksum ^= encode_once();
        checksum ^= decode_once();
    }

    const auto encode_started = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        checksum ^= encode_once();
    }
    const auto encode_elapsed = std::chrono::steady_clock::now() - encode_started;

    const auto decode_started = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        checksum ^= decode_once();
    }
    const auto decode_elapsed = std::chrono::steady_clock::now() - decode_started;

    g_checksum = checksum;


    const auto encode_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(encode_elapsed).count();
    const auto decode_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(decode_elapsed).count();
    const auto rows = flatten_bbo_frame_views(frame);
    const auto row_count = rows ? rows->size() : 0;
    std::cout << "{\"iterations\":" << iterations
              << ",\"warmup\":" << warmup
              << ",\"encode_ns_per_op\":" << (encode_ns / static_cast<double>(iterations))
              << ",\"decode_ns_per_op\":" << (decode_ns / static_cast<double>(iterations))
              << ",\"frame_bytes\":" << frame.size()
              << ",\"rows\":" << row_count
              << ",\"checksum\":" << g_checksum << "}\n";
    return EXIT_SUCCESS;
}
