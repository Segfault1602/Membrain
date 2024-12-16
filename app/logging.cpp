#include "logging.h"

#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"

namespace
{
constexpr const char* log_fmt_string = "[%s][%s] ";
constexpr const char* log_level_strings[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

SemaphoreHandle_t xSemaphore;
} // namespace

namespace logger
{
void init_logging()
{
    xSemaphore = xSemaphoreCreateMutex();
    printf("%c%c%c%c", 0x1B, 0x5B, 0x32, 0x4A);
    printf("%c[0;0H", 0x1b);
}

void log(LogLevel level, const char* filename, const char* fmt, ...)
{
    char buffer[MAX_LOG_MESSAGE_SIZE] = {0};
    int written =
        snprintf(buffer, MAX_LOG_MESSAGE_SIZE, log_fmt_string, log_level_strings[static_cast<int>(level)], filename);

    if (written < 0 || written >= MAX_LOG_MESSAGE_SIZE)
    {
        printf("Failed to write log message\n");
        return;
    }

    char* buffer_ptr = buffer + written;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer_ptr, MAX_LOG_MESSAGE_SIZE - written, fmt, args);
    va_end(args);

    printf(buffer);
}
} // namespace logger