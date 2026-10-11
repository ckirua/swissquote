#include <swissquote/clickhouse/client.hpp>
#include <swissquote/ingest/flush_retry_policy.hpp>
#include <swissquote/ingest/row_buffer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using swissquote::ingest::FlushRetryPolicy;

TEST_CASE("flush retry starts with a one second delay", "[ingest][retry]") {
    FlushRetryPolicy policy;

    CHECK(policy.eligible(0.0));
    policy.schedule_failure(100.0);

    CHECK(policy.next_retry_time_sec() == 101.0);
    CHECK_FALSE(policy.eligible(100.999));
    CHECK(policy.eligible(101.0));
}

TEST_CASE("flush retry doubles and caps its delay at thirty seconds", "[ingest][retry]") {
    FlushRetryPolicy policy;

    policy.schedule_failure(0.0);
    CHECK(policy.retry_delay_sec() == 2.0);
    policy.schedule_failure(1.0);
    CHECK(policy.retry_delay_sec() == 4.0);
    policy.schedule_failure(3.0);
    CHECK(policy.retry_delay_sec() == 8.0);
    policy.schedule_failure(7.0);
    CHECK(policy.retry_delay_sec() == 16.0);
    policy.schedule_failure(15.0);
    CHECK(policy.retry_delay_sec() == 30.0);
    policy.schedule_failure(31.0);
    CHECK(policy.retry_delay_sec() == 30.0);
}

TEST_CASE("flush retry success resets the deadline and delay", "[ingest][retry]") {
    FlushRetryPolicy policy;
    policy.schedule_failure(50.0);
    policy.schedule_failure(51.0);

    policy.reset_after_success();

    CHECK(policy.next_retry_time_sec() == 0.0);
    CHECK(policy.retry_delay_sec() == 1.0);
    CHECK(policy.eligible(0.0));
}

TEST_CASE("flush retry eligibility follows the deadline", "[ingest][retry]") {
    FlushRetryPolicy policy;
    policy.schedule_failure(20.0);

    CHECK_FALSE(policy.eligible(20.5));
    CHECK(policy.eligible(21.0));
}

TEST_CASE("flush retry permits one forced final attempt", "[ingest][retry]") {
    FlushRetryPolicy policy;
    policy.schedule_failure(200.0);

    CHECK_FALSE(policy.should_attempt(200.5));
    CHECK(policy.should_attempt(200.5, true));
}

TEST_CASE("row buffer owns source bytes and serializes stable views", "[ingest][row-buffer]") {
    std::string symbol = "EUR\tUSD";
    std::string platform = "plat\nform";
    std::string server = "srv\\x";
    std::string spread_profile = "raw";
    swissquote::ingest::RowBuffer buffer;
    buffer.reserve(1);
    buffer.append(swissquote::codec::BboRowView{
        .ts_ms = 1704067200123,
        .symbol = symbol,
        .platform = platform,
        .server = server,
        .spread_profile = spread_profile,
        .bid_price = 1.234567,
        .ask_price = 2.345678,
        .bid_spread = 0.1F,
        .ask_spread = 0.2F,
    });

    symbol = "changed";
    platform = "changed";
    server = "changed";
    spread_profile = "changed";

    const auto expected =
        "2024-01-01 00:00:00.123\tEUR\\\tUSD\tplat\\\nform\tsrv\\\\x\traw\t"
        "1.23457\t2.34568\t0.1\t0.2\n";
    CHECK(swissquote::clickhouse::serialize_rows_tsv(buffer) == expected);
    CHECK(swissquote::clickhouse::serialize_rows_tsv(buffer) == expected);
    CHECK(buffer.view(0).symbol == "EUR\tUSD");
}

TEST_CASE("row buffer clear retains an independent empty batch", "[ingest][row-buffer]") {
    swissquote::ingest::RowBuffer buffer;
    buffer.append(swissquote::codec::BboRowView{
        .ts_ms = 1704067200123,
        .symbol = "old",
        .platform = "old",
        .server = "old",
        .spread_profile = "old",
    });
    buffer.clear();
    CHECK(buffer.size() == 0);

    buffer.append(swissquote::codec::BboRowView{
        .ts_ms = 1704067201123,
        .symbol = "new",
        .platform = "new",
        .server = "new",
        .spread_profile = "new",
    });
    CHECK(buffer.size() == 1);
    CHECK(swissquote::clickhouse::serialize_rows_tsv(buffer) ==
          "2024-01-01 00:00:01.123\tnew\tnew\tnew\tnew\t0\t0\t0\t0\n");
}
