#include "swissquote/ingest/row_buffer.hpp"

namespace swissquote::ingest {

void RowBuffer::append(const codec::BboRow& row) {
    rows_.push_back(row);
}

void RowBuffer::append(codec::BboRow&& row) {
    rows_.push_back(std::move(row));
}

void RowBuffer::clear() {
    rows_.clear();
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
