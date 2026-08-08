#pragma once

#include "swissquote/codec/codec.hpp"

namespace swissquote::codec {

bool is_valid_bbo_row(const BboRow& row, std::int64_t now_ms);
bool is_valid_bbo_row(const BboRowView& row, std::int64_t now_ms);

} // namespace swissquote::codec
