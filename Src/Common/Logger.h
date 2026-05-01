#pragma once
//-----------------spdlog------------------
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//-----------------spdlog------------------

extern std::shared_ptr<spdlog::async_logger> logger;
void init_logger(const std::string& log_path = "logs/log.txt",
    spdlog::level::level_enum level = spdlog::level::info);

using Loglevel = spdlog::level::level_enum;

#ifdef _DEBUG
#define LOG_TRACE(...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::trace, __VA_ARGS__)

#define LOG_INFO(...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::info, __VA_ARGS__)

#define LOG_WARN(...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::warn, __VA_ARGS__)

#define LOG_ERROR(...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::err, __VA_ARGS__)

#define LOG_DEBUG(...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::debug, __VA_ARGS__)   

#else
#define LOG_TRACE(...)  logger->trace(__VA_ARGS__)
#define LOG_INFO(...)   logger->info(__VA_ARGS__)
#define LOG_WARN(...)   logger->warn(__VA_ARGS__)
#define LOG_ERROR(...)  logger->error(__VA_ARGS__)
#define LOG_DEBUG(...)  logger->debug(__VA_ARGS__)
#endif

#define LOG_INIT(PATH,LEVEL) init_logger(PATH,LEVEL)
#define LOG_SHUTDOWN() spdlog::shutdown();

