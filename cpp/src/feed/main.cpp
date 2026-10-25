#include "swissquote/config/config.hpp"
#include "swissquote/codec/codec.hpp"
#include "swissquote/net/http_client.hpp"
#include "swissquote/util/clock.hpp"
#include "swissquote/util/logging.hpp"
#include "swissquote/util/weekend_pause.hpp"
#include "swissquote/version.hpp"
#include "swissquote/ipc/unix_pub.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_running{true};

void on_signal(int) {
    g_running.store(false);
}

/// Sleep in short slices so SIGTERM/SIGINT can stop the process promptly.
void interruptible_sleep_ms(int total_ms) {
    constexpr int kSliceMs = 100;
    for (int elapsed = 0; elapsed < total_ms && g_running.load(); elapsed += kSliceMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSliceMs));
    }
}

class FeedWorkerPool {
    using Job = std::function<void(swissquote::net::QuotesClient&)>;
public:
    explicit FeedWorkerPool(std::size_t worker_count) {
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] { run(); });
        }
    }

    ~FeedWorkerPool() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        work_cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }

    FeedWorkerPool(const FeedWorkerPool&) = delete;
    FeedWorkerPool& operator=(const FeedWorkerPool&) = delete;

    void submit(Job job) {
        {
            std::lock_guard lock(mutex_);
            jobs_.push_back(std::move(job));
            ++pending_;
        }
        work_cv_.notify_one();
    }

    void wait_idle() {
        std::unique_lock lock(mutex_);
        idle_cv_.wait(lock, [this] { return pending_ == 0; });
    }

private:
    void run() {
        swissquote::net::QuotesClient http_client;
        while (true) {
            Job job;
            {
                std::unique_lock lock(mutex_);
                work_cv_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                if (stopping_ && jobs_.empty()) {
                    return;
                }
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }

            try {
                job(http_client);
            } catch (const std::exception& ex) {
                swissquote::util::log_warn(std::format("feed worker failed: {}", ex.what()));
            } catch (...) {
                swissquote::util::log_warn("feed worker failed with an unknown exception");
            }

            {
                std::lock_guard lock(mutex_);
                --pending_;
            }
            idle_cv_.notify_all();
        }
    }

    std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable idle_cv_;
    std::deque<Job> jobs_;
    std::vector<std::thread> workers_;
    std::size_t pending_{};
    bool stopping_{false};
};

} // namespace

int main(int argc, char** argv) {
    swissquote::config::CliOverrides cli;
    CLI::App app{"swissquote-feed — HTTP quotes → SBE → Unix IPC"};
    app.set_version_flag("-v,--version", SWISSQUOTE_VERSION);
    app.add_option("--config", cli.config_path, "path to service.toml");
    app.add_option("--socket-path", cli.socket_path, "Unix-domain seqpacket socket path");
    app.add_option("--log-level", cli.log_level, "log level");
    app.add_option("--log-dir", cli.log_dir, "log directory");
    CLI11_PARSE(app, argc, argv);

    const auto cfg = swissquote::config::load_config(cli);
    swissquote::util::init_logging(cfg.logging.level, cfg.logging.log_dir);

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    swissquote::ipc::ThreadedUnixSeqpacketPub pub(cfg.ipc.socket_path);
    pub.start();
    swissquote::util::log_info(std::format("publishing on Unix socket {}", cfg.ipc.socket_path));
    if (cfg.feed.weekend_pause) {
        swissquote::util::log_info(
            "weekend_pause enabled: skip REST polls Fri 16:55–Sun 16:50 America/New_York");
    }

    const auto symbols = swissquote::config::default_symbols();
    bool in_pause = false;
    constexpr std::size_t kFeedWorkerCount = 8;
    FeedWorkerPool workers(std::min(kFeedWorkerCount, symbols.size()));
    const auto elapsed_ms = [](const auto start) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    };

    while (g_running.load()) {
        const auto cycle_start = std::chrono::steady_clock::now();
        try {
            if (!pub.is_healthy()) {
                throw std::runtime_error("Unix IPC publisher is not healthy");
            }

            const auto now = std::chrono::system_clock::now();
            const bool should_pause =
                cfg.feed.weekend_pause && swissquote::util::in_weekend_pause(now);

            if (should_pause) {
                if (!in_pause) {
                    swissquote::util::log_info(
                        "entering weekend pause (Fri 16:55–Sun 16:50 America/New_York)");
                    in_pause = true;
                }
                interruptible_sleep_ms(5'000);
                swissquote::util::log_debug(std::format(
                    "poll cycle paused cycle_ms={}", elapsed_ms(cycle_start)));
                continue;
            }

            if (in_pause) {
                swissquote::util::log_info("exiting weekend pause; resuming REST polls");
                in_pause = false;
            }

            for (const auto symbol : symbols) {
                workers.submit([&, symbol](swissquote::net::QuotesClient& http_client) {
                    thread_local swissquote::codec::EncodeWorkspace encode_workspace;
                    auto http_ms = 0L;
                    auto encode_ms = 0L;
                    int http_status = 0;
                    std::string failure = "none";

                    try {
                        (void)swissquote::config::validate_symbol(symbol);

                        const auto http_start = std::chrono::steady_clock::now();
                        auto response = [&] {
                            try {
                                return http_client.fetch(symbol);
                            } catch (...) {
                                http_ms = elapsed_ms(http_start);
                                throw;
                            }
                        }();
                        http_ms = elapsed_ms(http_start);
                        if (!response) {
                            failure = "http_fetch";
                            swissquote::util::log_warn(
                                std::format("{}: HTTP fetch failed: {}", symbol, response.error()));
                        } else if (response->status != 200) {
                            http_status = response->status;
                            failure = "http_status";
                            swissquote::util::log_warn(
                                std::format("{}: HTTP status {}", symbol, response->status));
                        } else {
                            http_status = response->status;
                            const auto encode_start = std::chrono::steady_clock::now();
                            const auto fetch_ms =
                                static_cast<std::uint64_t>(swissquote::util::clock_realtime_ms());
                            auto encode_result = [&] {
                                try {
                                    return swissquote::codec::encode_bbo_frame_into(
                                        symbol, response->body, fetch_ms, encode_workspace);
                                } catch (...) {
                                    encode_ms = elapsed_ms(encode_start);
                                    throw;
                                }
                            }();
                            encode_ms = elapsed_ms(encode_start);
                            if (!encode_result) {
                                failure = "encode";
                                swissquote::util::log_warn(
                                    std::format("{}: encode failed", symbol));
                            } else {
                                pub.publish_nowait(encode_workspace.output);
                            }
                        }
                    } catch (const std::exception& ex) {
                        failure = "exception";
                        swissquote::util::log_warn(std::format("{}: {}", symbol, ex.what()));
                    }

                    swissquote::util::log_debug(std::format(
                        "symbol job {} http_status={} failure={} http_ms={} encode_ms={}",
                        symbol,
                        http_status,
                        failure,
                        http_ms,
                        encode_ms));
                });
            }
            workers.wait_idle();

            const auto stats = pub.stats();
            if (stats.dropped_queue || stats.dropped_no_client || stats.dropped_clients
                || stats.send_errors) {
                swissquote::util::log_warn(std::format(
                    "Unix IPC drops: queue={} no_client={} client={} send_errors={}",
                    stats.dropped_queue,
                    stats.dropped_no_client,
                    stats.dropped_clients,
                    stats.send_errors));
            }
            swissquote::util::log_info(std::format("published cycle for {} symbols", symbols.size()));
            swissquote::util::log_debug(
                std::format("poll cycle complete cycle_ms={}", elapsed_ms(cycle_start)));

            interruptible_sleep_ms(5'000);
        } catch (const std::exception& ex) {
            swissquote::util::log_error(std::format("emitter cycle failed: {}", ex.what()));
            swissquote::util::log_debug(
                std::format("poll cycle failed cycle_ms={}", elapsed_ms(cycle_start)));
            interruptible_sleep_ms(1'000);
        }
    }

    pub.close();
    swissquote::util::log_info("emitter stopped");
    return 0;
}
