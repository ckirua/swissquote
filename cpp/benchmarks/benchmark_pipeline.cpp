#include "fixtures.hpp"

#include <swissquote/clickhouse/client.hpp>
#include <swissquote/codec/codec.hpp>
#include <swissquote/codec/validate.hpp>
#include <swissquote/ingest/row_buffer.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
std::atomic<bool> g_count_allocations{false};
std::atomic<std::size_t> g_allocation_count{0};

void* allocate(std::size_t size) {
    if (void* ptr = std::malloc(size == 0 ? 1 : size)) {
        if (g_count_allocations.load(std::memory_order_relaxed)) {
            g_allocation_count.fetch_add(1, std::memory_order_relaxed);
        }
        return ptr;
    }
    throw std::bad_alloc();
}

void deallocate(void* ptr) noexcept {
    std::free(ptr);
}

} // namespace

void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void operator delete(void* ptr) noexcept {
    deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
    deallocate(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    deallocate(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    deallocate(ptr);
}

namespace {

using swissquote::codec::BboRow;
using swissquote::codec::BboRowView;
using swissquote::codec::EncodeWorkspace;
using swissquote::codec::encode_bbo_frame;
using swissquote::codec::encode_bbo_frame_into;
using swissquote::codec::flatten_bbo_frame;
using swissquote::codec::flatten_bbo_frame_views;
using swissquote::test::kSampleFetchTimeMs;
using swissquote::test::kSampleJson;
using swissquote::test::kSampleSymbol;

std::span<const std::byte> as_bytes(std::string_view value) {
    return {reinterpret_cast<const std::byte*>(value.data()), value.size()};
}

std::string make_small_json() {
    const auto next_venue = kSampleJson.find("},{\"topo\"");
    return "[" + std::string(kSampleJson.substr(1, next_venue)) + "]";
}

std::string make_large_json() {
    const std::string_view venue_list = kSampleJson.substr(1, kSampleJson.size() - 2);
    std::string result{"["};
    result.reserve(venue_list.size() * 24 + 23);
    for (int i = 0; i < 24; ++i) {
        if (i != 0) {
            result.push_back(',');
        }
        result.append(venue_list);
    }
    result.push_back(']');
    return result;
}

std::vector<BboRow> repeat_rows(const std::vector<BboRow>& source, std::size_t repetitions) {
    std::vector<BboRow> rows;
    rows.reserve(source.size() * repetitions);
    for (std::size_t i = 0; i < repetitions; ++i) {
        rows.insert(rows.end(), source.begin(), source.end());
    }
    return rows;
}

template <typename Operation>
std::size_t count_allocations(Operation&& operation, int iterations = 100) {
    for (int i = 0; i < 10; ++i) {
        static_cast<void>(operation());
    }

    g_allocation_count.store(0, std::memory_order_relaxed);
    g_count_allocations.store(true, std::memory_order_release);
    std::size_t sink = 0;
    for (int i = 0; i < iterations; ++i) {
        sink ^= operation();
    }
    g_count_allocations.store(false, std::memory_order_release);
    static volatile std::size_t benchmark_sink = 0;
    benchmark_sink ^= sink;
    return g_allocation_count.load(std::memory_order_relaxed) / static_cast<std::size_t>(iterations);
}

template <typename Operation>
void report_percentiles(std::string_view name, Operation&& operation, int iterations = 1000) {
    for (int i = 0; i < 10; ++i) {
        static_cast<void>(operation());
    }

    std::vector<std::int64_t> samples;
    samples.reserve(iterations);
    std::size_t sink = 0;
    for (int i = 0; i < iterations; ++i) {
        const auto started = std::chrono::steady_clock::now();
        sink ^= operation();
        const auto finished = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
                              finished - started)
                              .count());
    }
    std::sort(samples.begin(), samples.end());
    static volatile std::size_t benchmark_sink = 0;
    benchmark_sink ^= sink;

    const auto percentile = [&](std::size_t percentage) {
        const auto index = std::min(
            samples.size() - 1,
            (samples.size() * percentage) / 100);
        return samples[index];
    };
    std::cout << name
              << " latency ns: p50=" << percentile(50)
              << " p95=" << percentile(95)
              << " p99=" << percentile(99)
              << '\n';
}

TEST_CASE("codec benchmarks", "[benchmark][codec]") {
    const std::string small_json = make_small_json();
    const std::string large_json = make_large_json();
    const std::string malformed_json = R"({"malformed":)";

    const auto small_frame = encode_bbo_frame(
        kSampleSymbol, as_bytes(small_json), kSampleFetchTimeMs);
    const auto normal_frame = encode_bbo_frame(
        kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    const auto large_frame = encode_bbo_frame(
        kSampleSymbol, as_bytes(large_json), kSampleFetchTimeMs);
    REQUIRE(small_frame.has_value());
    REQUIRE(normal_frame.has_value());
    REQUIRE(large_frame.has_value());

    const auto small_rows = flatten_bbo_frame(*small_frame);
    const auto normal_rows = flatten_bbo_frame(*normal_frame);
    const auto large_rows = flatten_bbo_frame(*large_frame);
    REQUIRE(small_rows.has_value());
    REQUIRE(normal_rows.has_value());
    REQUIRE(large_rows.has_value());
    std::cout << "codec sizes: small bytes=" << small_frame->size()
              << " rows=" << small_rows->size()
              << " normal bytes=" << normal_frame->size()
              << " rows=" << normal_rows->size()
              << " large bytes=" << large_frame->size()
              << " rows=" << large_rows->size()
              << " malformed bytes=0 rows=0"
              << '\n';

    EncodeWorkspace reusable_encode_workspace;
    std::vector<BboRowView> reusable_view_rows;
    const auto benchmark_encode_reusable = [&](std::string_view json) {
        const auto result = encode_bbo_frame_into(
            kSampleSymbol, as_bytes(json), kSampleFetchTimeMs, reusable_encode_workspace);
        return result ? reusable_encode_workspace.output.size() : 0;
    };
    const auto benchmark_decode_views_reusable = [&](const std::vector<std::byte>& frame) {
        const auto result = flatten_bbo_frame_views(frame, reusable_view_rows);
        return result ? reusable_view_rows.size() : 0;
    };

    const auto benchmark_encode = [&](std::string_view json) {
        const auto result = encode_bbo_frame(kSampleSymbol, as_bytes(json), kSampleFetchTimeMs);
        return result ? result->size() : 0;
    };
    const auto benchmark_decode = [](const std::vector<std::byte>& frame) {
        const auto result = flatten_bbo_frame(frame);
        return result ? result->size() : 0;
    };
    const auto benchmark_decode_views = [](const std::vector<std::byte>& frame) {
        const auto result = flatten_bbo_frame_views(frame);
        return result ? result->size() : 0;
    };

    std::cout << "codec encode allocations/frame: small="
              << count_allocations([&] { return benchmark_encode(small_json); })
              << " normal=" << count_allocations([&] { return benchmark_encode(kSampleJson); })
              << " large=" << count_allocations([&] { return benchmark_encode(large_json); })
              << " malformed=" << count_allocations([&] { return benchmark_encode(malformed_json); })
              << '\n';
    std::cout << "codec decode allocations/frame: normal="
              << count_allocations([&] { return benchmark_decode(*normal_frame); })
              << " large=" << count_allocations([&] { return benchmark_decode(*large_frame); })
              << " malformed="
              << count_allocations([&] {
                     return flatten_bbo_frame(std::span<const std::byte>(
                                 normal_frame->data(), normal_frame->size() / 2))
                         .value_or(std::vector<BboRow>{})
                         .size();
                 })
              << '\n';
    std::cout << "codec view decode allocations/frame: normal="
              << count_allocations([&] { return benchmark_decode_views(*normal_frame); })
              << " large=" << count_allocations([&] { return benchmark_decode_views(*large_frame); })
              << '\n';
    std::cout << "reusable codec allocations/frame: encode="
              << count_allocations([&] { return benchmark_encode_reusable(kSampleJson); })
              << " view_decode="
              << count_allocations([&] { return benchmark_decode_views_reusable(*normal_frame); })
              << '\n';

    report_percentiles("codec encode small", [&] { return benchmark_encode(small_json); });
    report_percentiles("codec encode normal", [&] { return benchmark_encode(kSampleJson); });
    report_percentiles("codec encode large", [&] { return benchmark_encode(large_json); });
    report_percentiles("codec encode malformed", [&] { return benchmark_encode(malformed_json); });
    report_percentiles("codec decode normal", [&] { return benchmark_decode(*normal_frame); });
    report_percentiles("codec decode large", [&] { return benchmark_decode(*large_frame); });
    report_percentiles("codec decode malformed", [&] {
        return flatten_bbo_frame(std::span<const std::byte>(
                   normal_frame->data(), normal_frame->size() / 2))
            .value_or(std::vector<BboRow>{})
            .size();
    });
    report_percentiles("codec view decode normal", [&] {
        return benchmark_decode_views(*normal_frame);
    });
    report_percentiles("codec view decode large", [&] {
        return benchmark_decode_views(*large_frame);
    });

    BENCHMARK("encode small response") { return benchmark_encode(small_json); };
    BENCHMARK("encode normal response") { return benchmark_encode(kSampleJson); };
    BENCHMARK("encode large response") { return benchmark_encode(large_json); };
    BENCHMARK("encode malformed response") { return benchmark_encode(malformed_json); };
    BENCHMARK("decode normal frame") { return benchmark_decode(*normal_frame); };
    BENCHMARK("decode large frame") { return benchmark_decode(*large_frame); };
    BENCHMARK("decode normal frame views") { return benchmark_decode_views(*normal_frame); };
    BENCHMARK("decode large frame views") { return benchmark_decode_views(*large_frame); };
    BENCHMARK("decode malformed frame") {
        return flatten_bbo_frame(std::span<const std::byte>(
                   normal_frame->data(), normal_frame->size() / 2))
            .value_or(std::vector<BboRow>{})
            .size();
    };
}

TEST_CASE("writer pipeline benchmarks", "[benchmark][writer]") {
    const auto normal_frame = encode_bbo_frame(
        kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    const std::string large_json = make_large_json();
    const auto large_frame = encode_bbo_frame(
        kSampleSymbol, as_bytes(large_json), kSampleFetchTimeMs);
    REQUIRE(normal_frame.has_value());
    REQUIRE(large_frame.has_value());

    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    struct PipelineResult {
        std::size_t rows{};
        std::size_t payload_bytes{};
    };

    using RowBuffer = swissquote::ingest::RowBuffer;
    using ViewRows = std::vector<swissquote::codec::BboRowView>;

    const auto materialize_views = [&](const ViewRows& rows, RowBuffer& buffer) {
        buffer.clear();
        for (const auto& row : rows) {
            if (!swissquote::codec::is_valid_bbo_row(row, now_ms)) {
                continue;
            }
            buffer.append(swissquote::codec::BboRow{
                .ts_ms = row.ts_ms,
                .symbol = std::string(row.symbol),
                .platform = std::string(row.platform),
                .server = std::string(row.server),
                .spread_profile = std::string(row.spread_profile),
                .bid_price = row.bid_price,
                .ask_price = row.ask_price,
                .bid_spread = row.bid_spread,
                .ask_spread = row.ask_spread,
            });
        }
        return buffer.size();
    };

    const auto legacy_pipeline = [&](const std::vector<std::byte>& frame) {
        const auto decoded = flatten_bbo_frame(frame);
        if (!decoded) {
            return PipelineResult{};
        }

        swissquote::ingest::RowBuffer buffer;
        for (auto& row : *decoded) {
            if (swissquote::codec::is_valid_bbo_row(row, now_ms)) {
                buffer.append(row);
            }
        }
        return PipelineResult{
            .rows = buffer.size(),
            .payload_bytes = swissquote::clickhouse::serialize_rows_tsv(buffer.rows()).size(),
        };
    };

    const auto view_pipeline = [&](const std::vector<std::byte>& frame) {
        const auto decoded = swissquote::codec::flatten_bbo_frame_views(frame);
        if (!decoded) {
            return PipelineResult{};
        }

        RowBuffer buffer;
        materialize_views(*decoded, buffer);
        return PipelineResult{
            .rows = buffer.size(),
            .payload_bytes = swissquote::clickhouse::serialize_rows_tsv(buffer.rows()).size(),
        };
    };

    const auto legacy_payload = [&](const std::vector<std::byte>& frame) {
        return legacy_pipeline(frame).payload_bytes;
    };
    const auto view_payload = [&](const std::vector<std::byte>& frame) {
        return view_pipeline(frame).payload_bytes;
    };

    const auto normal_legacy = legacy_pipeline(*normal_frame);
    const auto normal_view = view_pipeline(*normal_frame);
    const auto large_legacy = legacy_pipeline(*large_frame);
    const auto large_view = view_pipeline(*large_frame);
    REQUIRE(normal_legacy.rows > 0);
    REQUIRE(large_legacy.rows > normal_legacy.rows);
    REQUIRE(normal_legacy.rows == normal_view.rows);
    REQUIRE(normal_legacy.payload_bytes == normal_view.payload_bytes);
    REQUIRE(large_legacy.rows == large_view.rows);
    REQUIRE(large_legacy.payload_bytes == large_view.payload_bytes);

    std::cout << "writer pipeline output: normal legacy_rows=" << normal_legacy.rows
              << " legacy_payload_bytes=" << normal_legacy.payload_bytes
              << " view_rows=" << normal_view.rows
              << " view_payload_bytes=" << normal_view.payload_bytes
              << " large legacy_rows=" << large_legacy.rows
              << " legacy_payload_bytes=" << large_legacy.payload_bytes
              << " view_rows=" << large_view.rows
              << " view_payload_bytes=" << large_view.payload_bytes
              << '\n';
    std::cout << "writer legacy allocations/frame: normal="
              << count_allocations([&] { return legacy_payload(*normal_frame); })
              << " large=" << count_allocations([&] { return legacy_payload(*large_frame); })
              << '\n';
    std::cout << "writer view allocations/frame: normal="
              << count_allocations([&] { return view_payload(*normal_frame); })
              << " large=" << count_allocations([&] { return view_payload(*large_frame); })
              << '\n';

    const auto normal_view_rows = swissquote::codec::flatten_bbo_frame_views(*normal_frame);
    const auto large_view_rows = swissquote::codec::flatten_bbo_frame_views(*large_frame);
    REQUIRE(normal_view_rows.has_value());
    REQUIRE(large_view_rows.has_value());
    RowBuffer retained_normal_buffer;
    RowBuffer retained_large_buffer;
    const auto materialize_fresh = [&](const ViewRows& rows) {
        RowBuffer buffer;
        return materialize_views(rows, buffer);
    };
    const auto materialize_retained_normal = [&] {
        return materialize_views(*normal_view_rows, retained_normal_buffer);
    };
    const auto materialize_retained_large = [&] {
        return materialize_views(*large_view_rows, retained_large_buffer);
    };
    static_cast<void>(materialize_retained_normal());
    static_cast<void>(materialize_retained_large());

    std::cout << "writer materialization allocations/batch: fresh_normal="
              << count_allocations([&] { return materialize_fresh(*normal_view_rows); })
              << " retained_normal="
              << count_allocations(materialize_retained_normal)
              << " fresh_large="
              << count_allocations([&] { return materialize_fresh(*large_view_rows); })
              << " retained_large="
              << count_allocations(materialize_retained_large)
              << '\n';
    report_percentiles("writer materialization fresh normal", [&] {
        return materialize_fresh(*normal_view_rows);
    });
    report_percentiles("writer materialization retained normal", materialize_retained_normal);
    report_percentiles("writer materialization fresh large", [&] {
        return materialize_fresh(*large_view_rows);
    });
    report_percentiles("writer materialization retained large", materialize_retained_large);

    const auto retained_normal_payload = [&] {
        return swissquote::clickhouse::serialize_rows_tsv(retained_normal_buffer.rows()).size();
    };
    const auto retained_large_payload = [&] {
        return swissquote::clickhouse::serialize_rows_tsv(retained_large_buffer.rows()).size();
    };
    std::cout << "writer serialization allocations/retained batch: normal="
              << count_allocations(retained_normal_payload)
              << " large=" << count_allocations(retained_large_payload)
              << '\n';
    report_percentiles("writer serialization retained normal", retained_normal_payload);
    report_percentiles("writer serialization retained large", retained_large_payload);

    report_percentiles("writer legacy normal", [&] {
        return legacy_payload(*normal_frame);
    });
    report_percentiles("writer legacy large", [&] {
        return legacy_payload(*large_frame);
    });
    report_percentiles("writer view normal", [&] {
        return view_payload(*normal_frame);
    });
    report_percentiles("writer view large", [&] {
        return view_payload(*large_frame);
    });

    BENCHMARK("writer legacy normal") { return legacy_payload(*normal_frame); };
    BENCHMARK("writer legacy large") { return legacy_payload(*large_frame); };
    BENCHMARK("writer view normal") { return view_payload(*normal_frame); };
    BENCHMARK("writer view large") { return view_payload(*large_frame); };
    BENCHMARK("writer materialization fresh normal") {
        return materialize_fresh(*normal_view_rows);
    };
    BENCHMARK("writer materialization fresh large") {
        return materialize_fresh(*large_view_rows);
    };
    BENCHMARK("writer materialization retained normal") {
        return materialize_retained_normal();
    };
    BENCHMARK("writer materialization retained large") {
        return materialize_retained_large();
    };
    BENCHMARK("writer serialization retained normal") {
        return retained_normal_payload();
    };
    BENCHMARK("writer serialization retained large") {
        return retained_large_payload();
    };
}

TEST_CASE("ClickHouse serialization benchmarks", "[benchmark][clickhouse]") {
    const auto frame = encode_bbo_frame(
        kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    REQUIRE(frame.has_value());
    const auto decoded = flatten_bbo_frame(*frame);
    REQUIRE(decoded.has_value());
    REQUIRE_FALSE(decoded->empty());

    const std::vector<BboRow> small_rows{decoded->front()};
    const std::vector<BboRow>& normal_rows = *decoded;
    const std::vector<BboRow> large_rows = repeat_rows(normal_rows, 20);

    const auto benchmark_serialize = [](const std::vector<BboRow>& rows) {
        return swissquote::clickhouse::serialize_rows_tsv(rows).size();
    };

    std::cout << "ClickHouse serialization allocations/batch: small="
              << count_allocations([&] { return benchmark_serialize(small_rows); })
              << " normal=" << count_allocations([&] { return benchmark_serialize(normal_rows); })
              << " large=" << count_allocations([&] { return benchmark_serialize(large_rows); })
              << '\n';
    std::cout << "ClickHouse payload bytes: small=" << benchmark_serialize(small_rows)
              << " normal=" << benchmark_serialize(normal_rows)
              << " large=" << benchmark_serialize(large_rows)
              << '\n';
    std::cout << "ClickHouse component allocations/call: timestamp="
              << count_allocations([&] {
                     return swissquote::clickhouse::detail::format_ts_ms(kSampleFetchTimeMs).size();
                 })
              << " escaped="
              << count_allocations([&] {
                     return swissquote::clickhouse::detail::escape_field("Swissquote\\\tvenue").size();
                 })
              << '\n';

    report_percentiles("ClickHouse serialize small", [&] {
        return benchmark_serialize(small_rows);
    });
    report_percentiles("ClickHouse serialize normal", [&] {
        return benchmark_serialize(normal_rows);
    });
    report_percentiles("ClickHouse serialize large", [&] {
        return benchmark_serialize(large_rows);
    });
    report_percentiles("ClickHouse format timestamp", [&] {
        return swissquote::clickhouse::detail::format_ts_ms(kSampleFetchTimeMs).size();
    });
    report_percentiles("ClickHouse escape field", [&] {
        return swissquote::clickhouse::detail::escape_field("Swissquote\\\tvenue").size();
    });

    BENCHMARK("serialize small batch") { return benchmark_serialize(small_rows); };
    BENCHMARK("serialize normal batch") { return benchmark_serialize(normal_rows); };
    BENCHMARK("serialize large batch") { return benchmark_serialize(large_rows); };
    BENCHMARK("format timestamp") {
        return swissquote::clickhouse::detail::format_ts_ms(kSampleFetchTimeMs).size();
    };
    BENCHMARK("escape field") {
        return swissquote::clickhouse::detail::escape_field("Swissquote\\\tvenue").size();
    };
}

} // namespace
