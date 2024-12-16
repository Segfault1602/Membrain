#pragma once

#include <stdio.h>

constexpr size_t MAX_LOG_MESSAGE_SIZE = 256;

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

#define LOG_DEBUG(fmt, ...)   logger::log(LogLevel::Debug, __FILE_NAME__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    logger::log(LogLevel::Info, __FILE_NAME__, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) logger::log(LogLevel::Warning, __FILE_NAME__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   logger::log(LogLevel::Error, __FILE_NAME__, fmt, ##__VA_ARGS__)

namespace logger
{
void init_logging();
void log(LogLevel level, const char* filename, const char* fmt, ...);
}