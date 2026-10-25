#include "swissquote/config/config.hpp"

#include <toml++/toml.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace swissquote::config {
namespace {

constexpr std::string_view kAllowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";

std::string env_or(const char* key, std::string_view fallback) {
    if (const char* val = std::getenv(key)) {
        return val;
    }
    return std::string(fallback);
}

std::filesystem::path default_config_path() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".config" / "service.toml";
    }
    return {};
}

void load_dotenv() {
    const auto path = env_or("DOTENV_PATH", "");
    if (path.empty()) {
        if (const char* home = std::getenv("HOME")) {
            const auto dotenv = std::filesystem::path(home) / ".env";
            if (!std::filesystem::exists(dotenv)) {
                return;
            }
            std::ifstream in(dotenv);
            std::string line;
            while (std::getline(in, line)) {
                const auto eq = line.find('=');
                if (eq == std::string::npos || line.starts_with('#')) {
                    continue;
                }
                const std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                if (!val.empty() && val.front() == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }
                setenv(key.c_str(), val.c_str(), 0);
            }
        }
        return;
    }
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        setenv(line.substr(0, eq).c_str(), line.substr(eq + 1).c_str(), 0);
    }
}

SwissquoteConfig from_toml(const toml::table& table) {
    SwissquoteConfig cfg;
    if (auto* ipc = table["ipc"].as_table()) {
        if (auto v = (*ipc)["socket_path"].value<std::string>()) {
            cfg.ipc.socket_path = *v;
        }
    }
    if (auto* logging = table["logging"].as_table()) {
        if (auto v = (*logging)["level"].value<std::string>()) {
            cfg.logging.level = *v;
        }
        if (auto v = (*logging)["log_dir"].value<std::string>()) {
            cfg.logging.log_dir = *v;
        }
    }
    if (auto* ch = table["clickhouse"].as_table()) {
        if (auto v = (*ch)["port"].value<int64_t>()) {
            cfg.clickhouse.native_port = static_cast<int>(*v);
        }
        if (auto v = (*ch)["user"].value<std::string>()) {
            cfg.clickhouse.user = *v;
        }
    }
    if (auto* writer = table["writer"].as_table()) {
        if (auto v = (*writer)["batch_size"].value<int64_t>()) {
            cfg.writer.batch_size = static_cast<int>(*v);
        }
        if (auto v = (*writer)["batch_interval_sec"].value<double>()) {
            cfg.writer.batch_interval_sec = *v;
        }
        if (auto v = (*writer)["recv_timeout_ms"].value<int64_t>()) {
            cfg.writer.recv_timeout_ms = static_cast<int>(*v);
        }
        if (auto v = (*writer)["startup_delay_sec"].value<double>()) {
            cfg.writer.startup_delay_sec = *v;
        }
        if (auto v = (*writer)["ch_database"].value<std::string>()) {
            cfg.writer.ch_database = *v;
        }
        if (auto v = (*writer)["ch_table"].value<std::string>()) {
            cfg.writer.ch_table = *v;
        }
        if (auto v = (*writer)["async_insert"].value<bool>()) {
            cfg.writer.async_insert = *v;
        }
    }
    if (auto* feed = table["feed"].as_table()) {
        if (auto v = (*feed)["weekend_pause"].value<bool>()) {
            cfg.feed.weekend_pause = *v;
        }
    }
    return cfg;
}

} // namespace


std::string validate_symbol(std::string_view symbol) {
    if (symbol.size() < 7 || symbol.size() > 13) {
        throw std::invalid_argument("invalid symbol");
    }
    if (std::count(symbol.begin(), symbol.end(), '/') != 1) {
        throw std::invalid_argument("invalid symbol");
    }
    for (const char ch : symbol) {
        if (kAllowed.find(ch) == std::string_view::npos) {
            throw std::invalid_argument("invalid symbol");
        }
    }
    return std::string(symbol);
}


SwissquoteConfig load_config(const CliOverrides& cli) {
    load_dotenv();
    SwissquoteConfig cfg;

    std::filesystem::path config_path = cli.config_path;
    if (config_path.empty()) {
        config_path = default_config_path();
    }
    if (!config_path.empty() && std::filesystem::exists(config_path)) {
        cfg = from_toml(toml::parse_file(config_path.string()));
    }

    if (!cli.socket_path.empty()) {
        cfg.ipc.socket_path = cli.socket_path;
    }
    if (!cli.log_level.empty()) {
        cfg.logging.level = cli.log_level;
    }
    if (!cli.log_dir.empty()) {
        cfg.logging.log_dir = cli.log_dir;
    }
    if (cli.batch_size >= 0) {
        cfg.writer.batch_size = cli.batch_size;
    }
    if (cli.batch_interval >= 0.0) {
        cfg.writer.batch_interval_sec = cli.batch_interval;
    }

    cfg.clickhouse.host = env_or("CLICKHOUSE_HOST", cfg.clickhouse.host);
    if (const char* port = std::getenv("CLICKHOUSE_PORT")) {
        cfg.clickhouse.native_port = std::stoi(port);
    }
    if (const char* http_port = std::getenv("CLICKHOUSE_HTTP_PORT")) {
        cfg.clickhouse.http_port = std::stoi(http_port);
    }
    cfg.clickhouse.user = env_or("CLICKHOUSE_USER", cfg.clickhouse.user);
    cfg.clickhouse.password = env_or("CLICKHOUSE_PASSWORD", cfg.clickhouse.password);

    return cfg;
}

std::vector<std::string_view> default_symbols() {
    static constexpr std::string_view kSymbols[] = {
        "XAU/AUD", "XAU/CHF", "XAU/EUR", "XAU/GBP", "XAU/USD", "XPD/USD", "XPT/USD",
        "XAG/AUD", "XAG/CHF", "XAG/EUR", "XAG/GBP", "XAG/USD", "AUD/CAD", "AUD/CHF",
        "AUD/JPY", "AUD/NZD", "AUD/USD", "CAD/CHF", "CAD/JPY", "CAD/SGD", "CHF/DKK",
        "CHF/HUF", "CHF/JPY", "CHF/NOK", "CHF/PLN", "CHF/SEK", "CHF/SGD", "DKK/SEK",
        "EUR/AUD", "EUR/CAD", "EUR/CHF", "EUR/CZK", "EUR/DKK", "EUR/GBP", "EUR/HKD",
        "EUR/HUF", "EUR/JPY", "EUR/NOK", "EUR/NZD", "EUR/PLN", "EUR/SEK", "EUR/SGD",
        "EUR/TRY", "EUR/USD", "EUR/ZAR", "GBP/AUD", "GBP/CAD", "GBP/CHF", "GBP/DKK",
        "GBP/HUF", "GBP/JPY", "GBP/NOK", "GBP/NZD", "GBP/PLN", "GBP/SEK",
        "GBP/SGD", "GBP/TRY", "GBP/USD", "HKD/JPY", "NOK/JPY", "NOK/SEK", "NZD/CAD",
        "NZD/CHF", "NZD/DKK", "NZD/JPY", "NZD/SEK", "NZD/SGD", "NZD/USD", "SEK/JPY",
        "SGD/HKD", "SGD/JPY", "TRY/JPY", "USD/CAD", "USD/CHF", "USD/CNH", "USD/CZK",
        "USD/DKK", "USD/HKD", "USD/HUF", "USD/ILS", "USD/JPY", "USD/MXN", "USD/NOK",
        "USD/PLN", "USD/SEK", "USD/SGD", "USD/TRY",
    };
    return {kSymbols, kSymbols + std::size(kSymbols)};
}

} // namespace swissquote::config
