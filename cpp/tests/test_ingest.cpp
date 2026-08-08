#include <swissquote/ingest/flush_retry_policy.hpp>

#include <catch2/catch_test_macros.hpp>

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
