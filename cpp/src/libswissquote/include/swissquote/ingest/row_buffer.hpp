#pragma once

#include "swissquote/codec/codec.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace swissquote::ingest {

class RowBuffer {
public:
    void reserve(std::size_t capacity);
    void append(const codec::BboRowView& row);
    void clear();
    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }
    [[nodiscard]] codec::BboRowView view(std::size_t index) const noexcept;

private:
    struct StringSlice {
        std::size_t offset{};
        std::size_t length{};
    };

    struct StoredRow {
        std::int64_t ts_ms{};
        StringSlice symbol;
        StringSlice platform;
        StringSlice server;
        StringSlice spread_profile;
        double bid_price{};
        double ask_price{};
        float bid_spread{};
        float ask_spread{};
    };

    [[nodiscard]] StringSlice append_string(std::string_view value);
    [[nodiscard]] std::string_view resolve(StringSlice slice) const noexcept;

    std::vector<StoredRow> rows_;
    std::vector<char> arena_;
};

[[nodiscard]] bool should_flush(
    std::size_t buf_len,
    int batch_size,
    double batch_interval_sec,
    double last_flush_sec,
    double now_sec);

} // namespace swissquote::ingest
