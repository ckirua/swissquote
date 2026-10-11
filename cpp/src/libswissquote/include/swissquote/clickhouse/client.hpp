#pragma once

#include "swissquote/config/config.hpp"
#include "swissquote/ingest/row_buffer.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
namespace swissquote::clickhouse {

struct InsertTimings {
    std::uint64_t serialization_ns{};
    std::uint64_t http_ns{};
};

namespace detail {
[[nodiscard]] std::string format_ts_ms(std::int64_t ts_ms);
[[nodiscard]] std::string escape_field(std::string_view value);
}

[[nodiscard]] std::string serialize_rows_tsv(const ingest::RowBuffer& rows);

class Client {
public:
    explicit Client(config::ClickHouseConfig cfg);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    [[nodiscard]] std::expected<void, std::string> insert_rows(
        const std::string& database,
        const std::string& table,
        const ingest::RowBuffer& rows,
        bool async_insert,
        InsertTimings* timings = nullptr) const;

private:
    struct Impl;
    config::ClickHouseConfig cfg_;
    std::unique_ptr<Impl> impl_;
};

} // namespace swissquote::clickhouse
