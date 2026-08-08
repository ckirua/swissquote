#pragma once

#include <algorithm>

namespace swissquote::ingest {

/// Tracks retry eligibility for failed flushes without owning a clock.
class FlushRetryPolicy {
public:
    static constexpr double initial_delay_sec = 1.0;
    static constexpr double max_delay_sec = 30.0;

    explicit FlushRetryPolicy(
        double initial_delay = initial_delay_sec,
        double max_delay = max_delay_sec) noexcept
        : initial_delay_sec_(initial_delay),
          max_delay_sec_(max_delay),
          retry_delay_sec_(initial_delay) {}

    [[nodiscard]] bool eligible(double now_sec) const noexcept {
        return now_sec >= next_retry_time_sec_;
    }

    [[nodiscard]] bool should_attempt(
        double now_sec, bool final_attempt = false) const noexcept {
        return final_attempt || eligible(now_sec);
    }

    void schedule_failure(double now_sec) noexcept {
        next_retry_time_sec_ = now_sec + retry_delay_sec_;
        retry_delay_sec_ = std::min(max_delay_sec_, retry_delay_sec_ * 2.0);
    }

    void reset_after_success() noexcept {
        next_retry_time_sec_ = 0.0;
        retry_delay_sec_ = initial_delay_sec_;
    }

    [[nodiscard]] double next_retry_time_sec() const noexcept {
        return next_retry_time_sec_;
    }

    [[nodiscard]] double retry_delay_sec() const noexcept { return retry_delay_sec_; }

private:
    double initial_delay_sec_;
    double max_delay_sec_;
    double next_retry_time_sec_ = 0.0;
    double retry_delay_sec_;
};

} // namespace swissquote::ingest
