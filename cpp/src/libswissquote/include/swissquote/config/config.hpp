#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace swissquote::config {

[[nodiscard]] std::string validate_zmq_host(std::string_view host);
[[nodiscard]] std::string validate_symbol(std::string_view symbol);

struct ZmqConfig {
    std::string host{"127.0.0.1"};
    int port{5559};
    [[nodiscard]] std::string endpoint() const;
};

struct LoggingConfig {
    std::string level{"INFO"};
    std::string log_dir;
};

struct ClickHouseConfig {
    std::string host;
    int native_port{9000};
    int http_port{8123};
    std::string user{"default"};
    std::string password;
};

struct WriterConfig {
    int batch_size{5000};
    double batch_interval_sec{0.1};
    int recv_timeout_ms{1000};
    double startup_delay_sec{0.5};
    std::string ch_database{"swissquote"};
    std::string ch_table{"bbo"};
    bool async_insert{true};
};

struct FeedConfig {
    /// Skip REST polls Fri 16:55–Sun 16:50 America/New_York when true.
    bool weekend_pause{true};
};

struct SwissquoteConfig {
    ZmqConfig zmq;
    LoggingConfig logging;
    ClickHouseConfig clickhouse;
    WriterConfig writer;
    FeedConfig feed;
};

struct CliOverrides {
    std::string config_path;
    std::string host;
    int port{-1};
    std::string log_level;
    std::string log_dir;
    int batch_size{-1};
    double batch_interval{-1.0};
};

SwissquoteConfig load_config(const CliOverrides& cli);
[[nodiscard]] std::vector<std::string_view> default_symbols();

} // namespace swissquote::config
