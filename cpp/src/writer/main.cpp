#include "swissquote/clickhouse/client.hpp"
#include "swissquote/codec/codec.hpp"
#include "swissquote/codec/validate.hpp"
#include "swissquote/config/config.hpp"
#include "swissquote/ingest/flush_retry_policy.hpp"
#include "swissquote/ingest/row_buffer.hpp"
#include "swissquote/util/clock.hpp"
#include "swissquote/util/logging.hpp"
#include "swissquote/version.hpp"
#include "swissquote/ipc/unix_sub.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void on_signal(int) {
    g_running.store(false);
}

} // namespace

int main(int argc, char** argv) {
    swissquote::config::CliOverrides cli;
    CLI::App app{"swissquote-writer — Unix IPC → decode → ClickHouse"};
    app.set_version_flag("-v,--version", SWISSQUOTE_VERSION);
    app.add_option("--config", cli.config_path, "path to service.toml");
    app.add_option("--socket-path", cli.socket_path, "Unix-domain seqpacket socket path");
    app.add_option("--log-level", cli.log_level, "log level");
    app.add_option("--log-dir", cli.log_dir, "log directory");
    app.add_option("--batch-size", cli.batch_size, "rows per flush");
    app.add_option("--batch-interval", cli.batch_interval, "max seconds between flushes");
    CLI11_PARSE(app, argc, argv);

    const auto cfg = swissquote::config::load_config(cli);
    swissquote::util::init_logging(cfg.logging.level, cfg.logging.log_dir);

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (cfg.writer.startup_delay_sec > 0) {
        std::this_thread::sleep_for(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<double>(cfg.writer.startup_delay_sec)));
    }

    swissquote::ipc::ThreadedUnixSeqpacketSub sub(cfg.ipc.socket_path);
    sub.start();

    swissquote::clickhouse::Client ch(cfg.clickhouse);
    using Clock = std::chrono::steady_clock;

    swissquote::ingest::RowBuffer buffer;
    std::vector<swissquote::codec::BboRowView> decoded_rows;
    if (cfg.writer.batch_size > 0) {
        buffer.reserve(static_cast<std::size_t>(cfg.writer.batch_size));
    }
    double last_successful_flush = swissquote::util::monotonic_sec();
    swissquote::ingest::FlushRetryPolicy retry_policy;
    const double recv_timeout = cfg.writer.recv_timeout_ms / 1000.0;

    std::uint64_t received = 0;
    std::uint64_t decode_errors = 0;
    std::uint64_t rejected_rows = 0;
    std::uint64_t rows_inserted = 0;
    std::uint64_t flush_count = 0;
    std::uint64_t decode_ns = 0;
    std::uint64_t materialization_ns = 0;

    constexpr auto stats_interval = std::chrono::seconds{60};
    auto stats_started = Clock::now();
    auto previous_sub_stats = sub.stats();
    std::uint64_t interval_frames = 0;
    std::uint64_t interval_valid_frames = 0;
    std::uint64_t interval_rows_buffered = 0;
    std::uint64_t interval_rows_inserted = 0;
    std::uint64_t interval_decode_errors = 0;
    std::uint64_t interval_rejected_rows = 0;
    std::uint64_t interval_flush_attempts = 0;
    std::uint64_t interval_flush_successes = 0;
    std::uint64_t interval_flush_failures = 0;
    std::uint64_t interval_decode_ns = 0;
    std::uint64_t interval_decode_max_ns = 0;
    std::uint64_t interval_materialization_ns = 0;
    std::uint64_t interval_materialization_max_ns = 0;
    std::uint64_t interval_serialization_ns = 0;
    std::uint64_t interval_serialization_max_ns = 0;
    std::uint64_t interval_http_ns = 0;
    std::uint64_t interval_http_max_ns = 0;
    std::size_t interval_max_frame_bytes = 0;
    std::size_t interval_max_queue_size = previous_sub_stats.queue_size;

    const auto elapsed_ns = [](const Clock::time_point start) {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
    };

    const auto average_us = [](std::uint64_t ns, std::uint64_t count) {
        return count == 0 ? 0.0 : static_cast<double>(ns) / (1000.0 * count);
    };

    const auto ring_buffer_max_bytes =
        static_cast<std::uint64_t>(swissquote::ipc::ThreadedUnixSeqpacketSub::kRingCapacity) *
        (swissquote::ipc::ThreadedUnixSeqpacketSub::kMaxPayload + sizeof(std::uint32_t));

    swissquote::util::log_info(std::format(
        "writer started ipc={} ch={}.{} batch={} interval={:.1f}s "
        "queue_slots={} queue_max_frame_bytes={} queue_max_buffer_bytes={}",
        cfg.ipc.socket_path,
        cfg.writer.ch_database,
        cfg.writer.ch_table,
        cfg.writer.batch_size,
        cfg.writer.batch_interval_sec,
        swissquote::ipc::ThreadedUnixSeqpacketSub::kRingCapacity,
        swissquote::ipc::ThreadedUnixSeqpacketSub::kMaxPayload,
        ring_buffer_max_bytes));
    auto flush = [&](bool final_attempt = false) {
        if (buffer.size() == 0) {
            return;
        }

        const double now = swissquote::util::monotonic_sec();
        if (!retry_policy.should_attempt(now, final_attempt)) {
            return;
        }

        const auto rows_to_flush = buffer.size();
        swissquote::clickhouse::InsertTimings insert_timings{};
        ++interval_flush_attempts;
        auto result = ch.insert_rows(
            cfg.writer.ch_database,
            cfg.writer.ch_table,
            buffer,
            cfg.writer.async_insert,
            &insert_timings);
        interval_serialization_ns += insert_timings.serialization_ns;
        interval_serialization_max_ns =
            std::max(interval_serialization_max_ns, insert_timings.serialization_ns);
        interval_http_ns += insert_timings.http_ns;
        interval_http_max_ns = std::max(interval_http_max_ns, insert_timings.http_ns);
        if (!result) {
            ++interval_flush_failures;
            swissquote::util::log_error(std::format("flush failed: {}", result.error()));
            retry_policy.schedule_failure(swissquote::util::monotonic_sec());
            return;
        }

        rows_inserted += rows_to_flush;
        interval_rows_inserted += rows_to_flush;
        ++flush_count;
        ++interval_flush_successes;
        swissquote::util::log_debug(std::format(
            "flushed {} rows decode_ns={} materialization_ns={} serialization_ns={} http_ns={}",
            rows_to_flush,
            decode_ns,
            materialization_ns,
            insert_timings.serialization_ns,
            insert_timings.http_ns));
        buffer.clear();
        last_successful_flush = swissquote::util::monotonic_sec();
        retry_policy.reset_after_success();
        decode_ns = 0;
        materialization_ns = 0;
    };


    while (g_running.load()) {
        sub.consume_wait_frame(recv_timeout, [&](std::span<const std::byte> frame) {
            ++interval_frames;
            interval_max_frame_bytes = std::max(interval_max_frame_bytes, frame.size());
            const auto decode_start = Clock::now();
            auto decode_result =
                swissquote::codec::flatten_bbo_frame_views(frame, decoded_rows);
            const auto frame_decode_ns = elapsed_ns(decode_start);
            decode_ns += frame_decode_ns;
            interval_decode_ns += frame_decode_ns;
            interval_decode_max_ns = std::max(interval_decode_max_ns, frame_decode_ns);
            if (!decode_result) {
                decode_errors += 1;
                ++interval_decode_errors;
                return;
            }

            received += 1;
            ++interval_valid_frames;
            const auto materialization_start = Clock::now();
            const auto now_ms = static_cast<std::int64_t>(swissquote::util::clock_realtime_ms());
            for (const auto& row : decoded_rows) {
                if (!swissquote::codec::is_valid_bbo_row(row, now_ms)) {
                    rejected_rows += 1;
                    ++interval_rejected_rows;
                    continue;
                }
                buffer.append(row);
                ++interval_rows_buffered;
            }
            const auto frame_materialization_ns = elapsed_ns(materialization_start);
            materialization_ns += frame_materialization_ns;
            interval_materialization_ns += frame_materialization_ns;
            interval_materialization_max_ns =
                std::max(interval_materialization_max_ns, frame_materialization_ns);
        });

        const double now = swissquote::util::monotonic_sec();
        if (swissquote::ingest::should_flush(
                buffer.size(),
                cfg.writer.batch_size,
                cfg.writer.batch_interval_sec,
                last_successful_flush,
                now)) {
            flush();
        }

        const auto sub_stats = sub.stats();
        interval_max_queue_size = std::max(interval_max_queue_size, sub_stats.queue_size);
        const auto stats_now = Clock::now();
        const auto elapsed = std::chrono::duration<double>(stats_now - stats_started).count();
        if (stats_now - stats_started >= stats_interval) {
            const auto received_delta = sub_stats.received - previous_sub_stats.received;
            const auto push_failures_delta =
                sub_stats.dropped_queue - previous_sub_stats.dropped_queue;
            const auto recv_errors_delta =
                sub_stats.recv_errors - previous_sub_stats.recv_errors;
            const auto avg_http_ms = interval_flush_attempts == 0
                ? 0.0
                : static_cast<double>(interval_http_ns) /
                      (1'000'000.0 * interval_flush_attempts);
            const auto avg_serialization_ms = interval_flush_attempts == 0
                ? 0.0
                : static_cast<double>(interval_serialization_ns) /
                      (1'000'000.0 * interval_flush_attempts);

            swissquote::util::log_info(std::format(
                "writer stats window_s={:.1f} sub_enqueued_fps={:.1f} "
                "consumed_fps={:.1f} http_success_rows_per_s={:.1f} "
                "rows_materialized={} batch_pending_rows={} queue_depth={}/{} queue_sampled_max={} "
                "queue_push_attempt_failures={} recv_errors={} max_frame_bytes={} "
                "decode_errors={} rejected_rows={} flushes={}/{} failures={} "
                "decode_avg_us={:.1f} decode_max_us={:.1f} "
                "materialize_avg_us={:.1f} materialize_max_us={:.1f} "
                "serialize_avg_ms={:.2f} serialize_max_ms={:.2f} "
                "http_avg_ms={:.2f} http_max_ms={:.2f}",
                elapsed,
                received_delta / elapsed,
                interval_frames / elapsed,
                interval_rows_inserted / elapsed,
                interval_rows_buffered,
                buffer.size(),
                sub_stats.queue_size,
                swissquote::ipc::ThreadedUnixSeqpacketSub::kRingCapacity,
                interval_max_queue_size,
                push_failures_delta,
                recv_errors_delta,
                interval_max_frame_bytes,
                interval_decode_errors,
                interval_rejected_rows,
                interval_flush_successes,
                interval_flush_attempts,
                interval_flush_failures,
                average_us(interval_decode_ns, interval_frames),
                interval_decode_max_ns / 1000.0,
                average_us(interval_materialization_ns, interval_valid_frames),
                interval_materialization_max_ns / 1000.0,
                avg_serialization_ms,
                interval_serialization_max_ns / 1'000'000.0,
                avg_http_ms,
                interval_http_max_ns / 1'000'000.0));

            stats_started = stats_now;
            previous_sub_stats = sub_stats;
            interval_frames = 0;
            interval_valid_frames = 0;
            interval_rows_buffered = 0;
            interval_rows_inserted = 0;
            interval_decode_errors = 0;
            interval_rejected_rows = 0;
            interval_flush_attempts = 0;
            interval_flush_successes = 0;
            interval_flush_failures = 0;
            interval_decode_ns = 0;
            interval_decode_max_ns = 0;
            interval_materialization_ns = 0;
            interval_materialization_max_ns = 0;
            interval_serialization_ns = 0;
            interval_serialization_max_ns = 0;
            interval_http_ns = 0;
            interval_http_max_ns = 0;
            interval_max_frame_bytes = 0;
            interval_max_queue_size = sub_stats.queue_size;
        }
    }

    flush(true);
    sub.close();

    swissquote::util::log_info(std::format(
        "writer stopped received={} inserted={} rejected={} decode_errors={} flushes={}",
        received,
        rows_inserted,
        rejected_rows,
        decode_errors,
        flush_count));
    return 0;
}
