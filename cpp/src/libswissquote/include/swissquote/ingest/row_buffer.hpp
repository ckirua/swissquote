#pragma once

#include "swissquote/codec/codec.hpp"

#include <utility>
#include <vector>

namespace swissquote::ingest {

class RowBuffer {
public:
    void reserve(std::size_t capacity);
    void append(const codec::BboRow& row);

    void append(codec::BboRow&& row);
    void clear();
    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }
    [[nodiscard]] const std::vector<codec::BboRow>& rows() const noexcept { return rows_; }

private:
    std::vector<codec::BboRow> rows_;
};

[[nodiscard]] bool should_flush(
    std::size_t buf_len,
    int batch_size,
    double batch_interval_sec,
    double last_flush_sec,
    double now_sec);

} // namespace swissquote::ingest
