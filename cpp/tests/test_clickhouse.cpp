#include <swissquote/clickhouse/client.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

TEST_CASE("ClickHouse TSV serialization preserves UTC timestamp layout") {
    const std::vector<swissquote::codec::BboRow> rows{{
        .ts_ms = 1577934245006,
        .symbol = "EUR/USD",
        .platform = "SwissquoteCapitalMarkets",
        .server = "Live7",
        .spread_profile = "prime",
        .bid_price = 1.23456,
        .ask_price = 1.23486,
        .bid_spread = 0.4F,
        .ask_spread = 0.4F,
    }};

    CHECK(swissquote::clickhouse::serialize_rows_tsv(rows)
          == "2020-01-02 03:04:05.006\tEUR/USD\tSwissquoteCapitalMarkets\tLive7\tprime"
             "\t1.23456\t1.23486\t0.4\t0.4\n");
}
