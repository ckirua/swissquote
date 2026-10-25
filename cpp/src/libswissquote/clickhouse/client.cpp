#include "swissquote/clickhouse/client.hpp"

#include <charconv>
#include <chrono>
#include <ctime>
#include <format>
#include <iterator>
#include <limits>
#include <stdexcept>
#ifdef SWISSQUOTE_HAVE_OPENSSL
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

namespace swissquote::clickhouse {
namespace detail {

std::string format_ts_ms(std::int64_t ts_ms) {
    const std::chrono::sys_time<std::chrono::milliseconds> tp{std::chrono::milliseconds(ts_ms)};
    const std::time_t tt = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp));
    const auto ms = ts_ms % 1000;
    std::tm tm{};
    gmtime_r(&tt, &tm);
    return std::format(
        "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec,
        ms);
}

std::string escape_field(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        if (ch == '\\' || ch == '\t' || ch == '\n') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

namespace {
template <typename Integer>
void append_zero_padded(std::string& out, Integer value, std::size_t width) {
    char buffer[32];
    const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("failed to format ClickHouse timestamp");
    }
    const bool negative = buffer[0] == '-';
    const char* digits = buffer + (negative ? 1 : 0);
    const auto digit_count = static_cast<std::size_t>(result.ptr - digits);
    const auto sign_width = static_cast<std::size_t>(negative);
    const auto padding = width > digit_count + sign_width
        ? width - digit_count - sign_width
        : 0;
    if (negative) {
        out.push_back('-');
    }
    out.append(padding, '0');
    out.append(digits, digit_count);
}
} // namespace

void append_ts_ms(std::string& out, std::int64_t ts_ms) {
    const std::chrono::sys_time<std::chrono::milliseconds> tp{std::chrono::milliseconds(ts_ms)};
    const std::time_t tt = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp));
    const auto ms = ts_ms % 1000;
    std::tm tm{};
    gmtime_r(&tt, &tm);
    append_zero_padded(out, tm.tm_year + 1900, 4);
    out.push_back('-');
    append_zero_padded(out, tm.tm_mon + 1, 2);
    out.push_back('-');
    append_zero_padded(out, tm.tm_mday, 2);
    out.push_back(' ');
    append_zero_padded(out, tm.tm_hour, 2);
    out.push_back(':');
    append_zero_padded(out, tm.tm_min, 2);
    out.push_back(':');
    append_zero_padded(out, tm.tm_sec, 2);
    out.push_back('.');
    append_zero_padded(out, ms, 3);
}

void append_escaped_field(std::string& out, std::string_view value) {
    for (const char ch : value) {
        if (ch == '\\' || ch == '\t' || ch == '\n') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
}

template <typename Number>
void append_number(std::string& out, Number value) {
    char buffer[64];
    const auto result = std::to_chars(
        std::begin(buffer),
        std::end(buffer),
        value,
        std::chars_format::general,
        6);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("failed to format numeric ClickHouse field");
    }
    out.append(buffer, result.ptr);
}

} // namespace detail

namespace {

void serialize_rows_tsv_into(
    std::string& body,
    const ingest::RowBuffer& rows) {
    body.clear();
    if (rows.size() <= std::numeric_limits<std::size_t>::max() / 96 &&
        body.capacity() < rows.size() * 96) {
        body.reserve(rows.size() * 96);
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto row = rows.view(index);
        detail::append_ts_ms(body, row.ts_ms);
        body.push_back('\t');
        detail::append_escaped_field(body, row.symbol);
        body.push_back('\t');
        detail::append_escaped_field(body, row.platform);
        body.push_back('\t');
        detail::append_escaped_field(body, row.server);
        body.push_back('\t');
        detail::append_escaped_field(body, row.spread_profile);
        body.push_back('\t');
        detail::append_number(body, row.bid_price);
        body.push_back('\t');
        detail::append_number(body, row.ask_price);
        body.push_back('\t');
        detail::append_number(body, row.bid_spread);
        body.push_back('\t');
        detail::append_number(body, row.ask_spread);
        body.push_back('\n');
    }
}

} // namespace

std::string serialize_rows_tsv(const ingest::RowBuffer& rows) {
    std::string body;
    serialize_rows_tsv_into(body, rows);
    return body;
}


struct Client::Impl {
    httplib::Client client;
    std::string payload;

    explicit Impl(const config::ClickHouseConfig& cfg)
        : client(cfg.host, cfg.http_port) {
        client.set_connection_timeout(10, 0);
        client.set_keep_alive(true);
        client.set_read_timeout(60, 0);
    }
};


Client::Client(config::ClickHouseConfig cfg)
    : cfg_(std::move(cfg)), impl_(std::make_unique<Impl>(cfg_)) {}

Client::~Client() = default;

Client::Client(Client&&) noexcept = default;

Client& Client::operator=(Client&&) noexcept = default;

std::expected<void, std::string> Client::insert_rows(
    const std::string& database,
    const std::string& table,
    const ingest::RowBuffer& rows,
    bool async_insert,
    InsertTimings* timings) const {
    if (timings) {
        *timings = {};
    }
    if (cfg_.host.empty()) {
        return std::unexpected("CLICKHOUSE_HOST is not set");
    }
    if (rows.size() == 0) {
        return {};
    }

    const auto serialization_start = std::chrono::steady_clock::now();
    serialize_rows_tsv_into(impl_->payload, rows);
    if (timings) {
        timings->serialization_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - serialization_start)
                .count());
    }

    std::string query = std::format(
        "INSERT INTO {}.{} (ts, symbol, platform, server, spread_profile, "
        "bid_price, ask_price, bid_spread, ask_spread) FORMAT TabSeparated",
        database,
        table);

    std::string path = std::format("/?query={}", httplib::detail::encode_query_param(query));
    if (async_insert) {
        path += "&async_insert=1&wait_for_async_insert=0";
    }
    if (!cfg_.user.empty()) {
        path += std::format("&user={}", httplib::detail::encode_query_param(cfg_.user));
    }
    if (!cfg_.password.empty()) {
        path += std::format("&password={}", httplib::detail::encode_query_param(cfg_.password));
    }
    const auto http_start = std::chrono::steady_clock::now();
    auto res = impl_->client.Post(path, impl_->payload, "text/tab-separated-values");
    if (timings) {
        timings->http_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - http_start)
                .count());
    }
    if (!res) {
        return std::unexpected(std::string("ClickHouse insert failed: ") + httplib::to_string(res.error()));
    }
    if (res->status >= 400) {
        return std::unexpected(std::format("ClickHouse HTTP {}: {}", res->status, res->body));
    }
    return {};
}

} // namespace swissquote::clickhouse
