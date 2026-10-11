#include "swissquote/ingest/row_buffer.hpp"

#include <limits>

namespace swissquote::ingest {

void RowBuffer::reserve(const std::size_t capacity) {
    rows_.reserve(capacity);
    if (capacity <= std::numeric_limits<std::size_t>::max() / 96) {
        arena_.reserve(capacity * 96);
    }
}

RowBuffer::StringSlice RowBuffer::append_string(const std::string_view value) {
    const auto offset = arena_.size();
    arena_.insert(arena_.end(), value.begin(), value.end());
    return {.offset = offset, .length = value.size()};
}

void RowBuffer::append(const codec::BboRowView& row) {
    StoredRow stored{
        .ts_ms = row.ts_ms,
        .symbol = append_string(row.symbol),
        .platform = append_string(row.platform),
        .server = append_string(row.server),
        .spread_profile = append_string(row.spread_profile),
        .bid_price = row.bid_price,
        .ask_price = row.ask_price,
        .bid_spread = row.bid_spread,
        .ask_spread = row.ask_spread,
    };
    rows_.push_back(stored);
}

std::string_view RowBuffer::resolve(const StringSlice slice) const noexcept {
    if (slice.length == 0) {
        return {};
    }
    return std::string_view(arena_.data() + slice.offset, slice.length);
}

codec::BboRowView RowBuffer::view(const std::size_t index) const noexcept {
    const auto& row = rows_[index];
    return {
        .ts_ms = row.ts_ms,
        .symbol = resolve(row.symbol),
        .platform = resolve(row.platform),
        .server = resolve(row.server),
        .spread_profile = resolve(row.spread_profile),
        .bid_price = row.bid_price,
        .ask_price = row.ask_price,
        .bid_spread = row.bid_spread,
        .ask_spread = row.ask_spread,
    };
}

void RowBuffer::clear() {
    rows_.clear();
    arena_.clear();
}

bool should_flush(
    std::size_t buf_len,
    int batch_size,
    double batch_interval_sec,
    double last_flush_sec,
    double now_sec) {
    if (buf_len >= static_cast<std::size_t>(batch_size)) {
        return true;
    }
    if (buf_len == 0) {
        return false;
    }
    return (now_sec - last_flush_sec) >= batch_interval_sec;
}

} // namespace swissquote::ingest
