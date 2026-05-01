#include "stdafx.h"
#include "Logger.h"

std::shared_ptr<spdlog::async_logger> logger;

void inline init_console_utf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

void init_logger(const std::string& log_path, spdlog::level::level_enum level) {
    init_console_utf8();
    spdlog::init_thread_pool(8192, 1);

#ifdef LOG_TO_FILE
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path, true);

#ifdef _DEBUG
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%s:%!:%#] [%^%l%$]  %v");
#else
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] %v");
#endif

#endif

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef _DEBUG
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%s:%!:%#] [%^%l%$]  %v");
#else
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] %v");
#endif


#ifdef LOG_TO_FILE
    logger = std::make_shared<spdlog::async_logger>(
        "async_logger", spdlog::sinks_init_list{ console_sink, file_sink }, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
#else
    logger = std::make_shared<spdlog::async_logger>(
        "async_logger", spdlog::sinks_init_list{ console_sink }, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
#endif
    logger->set_level(level);
    spdlog::register_logger(logger);
}