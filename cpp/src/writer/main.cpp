#include "swissquote/clickhouse/client.hpp"
#include "swissquote/codec/codec.hpp"
#include "swissquote/codec/validate.hpp"
#include "swissquote/config/config.hpp"
#include "swissquote/ingest/flush_retry_policy.hpp"
#include "swissquote/ingest/row_buffer.hpp"
#include "swissquote/util/clock.hpp"
#include "swissquote/util/logging.hpp"
#include "swissquote/version.hpp"
#include "swissquote/zmq/sub.hpp"

#include <CLI/CLI.hpp>

#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void on_signal(int) {
    g_running.store(false);
}

} // namespace

int main(int argc, char** argv) {
    swissquote::config::CliOverrides cli;
    CLI::App app{"swissquote-writer — ZMQ SUB → decode → ClickHouse"};
    app.set_version_flag("-v,--version", SWISSQUOTE_VERSION);
    app.add_option("--config", cli.config_path, "path to service.toml");
    app.add_option("--host", cli.host, "ZMQ connect host");
    app.add_option("--port", cli.port, "ZMQ port");
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

    swissquote::zmq::ThreadedZmqSub sub(cfg.zmq.endpoint());
    sub.start();

    swissquote::clickhouse::Client ch(cfg.clickhouse);
    using Clock = std::chrono::steady_clock;

    swissquote::ingest::RowBuffer buffer;
    std::vector<swissquote::codec::BboRowView> decoded_rows;
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

    const auto elapsed_ns = [](const Clock::time_point start) {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
    };

    swissquote::util::log_info(std::format(
        "writer started sub={} ch={}.{} batch={} interval={:.1f}s",
        cfg.zmq.endpoint(),
        cfg.writer.ch_database,
        cfg.writer.ch_table,
        cfg.writer.batch_size,
        cfg.writer.batch_interval_sec));

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
        auto result = ch.insert_rows(
            cfg.writer.ch_database,
            cfg.writer.ch_table,
            buffer.rows(),
            cfg.writer.async_insert,
            &insert_timings);
        if (!result) {
            swissquote::util::log_error(std::format("flush failed: {}", result.error()));
            retry_policy.schedule_failure(swissquote::util::monotonic_sec());
            return;
        }

        rows_inserted += rows_to_flush;
        flush_count += 1;
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
            const auto decode_start = Clock::now();
            auto decode_result =
                swissquote::codec::flatten_bbo_frame_views(frame, decoded_rows);
            decode_ns += elapsed_ns(decode_start);
            if (!decode_result) {
                decode_errors += 1;
                return;
            }

            received += 1;
            const auto materialization_start = Clock::now();
            const auto now_ms = static_cast<std::int64_t>(swissquote::util::clock_realtime_ms());
            for (const auto& row : decoded_rows) {
                if (!swissquote::codec::is_valid_bbo_row(row, now_ms)) {
                    rejected_rows += 1;
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
            materialization_ns += elapsed_ns(materialization_start);
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
