#pragma once

#include <string>

namespace swissquote::util {

void init_logging(const std::string& level, const std::string& log_dir = {});
void log_info(const std::string& msg);
void log_warn(const std::string& msg);
void log_error(const std::string& msg);
void log_debug(const std::string& msg);

} // namespace swissquote::util
