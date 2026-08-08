#include "swissquote/util/logging.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <stdexcept>

namespace swissquote::util {
namespace {

spdlog::level::level_enum parse_level(const std::string& level) {
    if (level == "TRACE") return spdlog::level::trace;
    if (level == "DEBUG") return spdlog::level::debug;
    if (level == "INFO") return spdlog::level::info;
    if (level == "WARN" || level == "WARNING") return spdlog::level::warn;
    if (level == "ERROR") return spdlog::level::err;
    if (level == "CRITICAL") return spdlog::level::critical;
    return spdlog::level::info;
}

} // namespace

void init_logging(const std::string& level, const std::string& log_dir) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (!log_dir.empty()) {
        std::filesystem::create_directories(log_dir);
        const auto path = (std::filesystem::path(log_dir) / "swissquote.log").string();
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true));
    }
    auto logger = std::make_shared<spdlog::logger>("swissquote", sinks.begin(), sinks.end());
    logger->set_level(parse_level(level));
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_default_logger(std::move(logger));
}

void log_info(const std::string& msg) { spdlog::info(msg); }
void log_warn(const std::string& msg) { spdlog::warn(msg); }
void log_error(const std::string& msg) { spdlog::error(msg); }
void log_debug(const std::string& msg) { spdlog::debug(msg); }

} // namespace swissquote::util
