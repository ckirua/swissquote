#pragma once

#include "swissquote/codec/codec.hpp"
#include "swissquote/config/config.hpp"

#include <cstdint>
#include <string_view>
#include <expected>
#include <string>
#include <memory>
#include <vector>

namespace swissquote::clickhouse {

struct InsertTimings {
    std::uint64_t serialization_ns{};
    std::uint64_t http_ns{};
};

namespace detail {
[[nodiscard]] std::string format_ts_ms(std::int64_t ts_ms);
[[nodiscard]] std::string escape_field(std::string_view value);
}

[[nodiscard]] std::string serialize_rows_tsv(const std::vector<codec::BboRow>& rows);

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
        const std::vector<codec::BboRow>& rows,
        bool async_insert,
        InsertTimings* timings = nullptr) const;

private:
    struct Impl;
    config::ClickHouseConfig cfg_;
    std::unique_ptr<Impl> impl_;
};

} // namespace swissquote::clickhouse
