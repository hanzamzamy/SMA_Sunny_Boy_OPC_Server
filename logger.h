#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum { LOG_LEVEL_ERROR, LOG_LEVEL_WARN, LOG_LEVEL_INFO, LOG_LEVEL_DEBUG } log_level_t;

/**
 * @brief Initializes the file logger.
 *
 * @param filename The path to the log file.
 * @param level The maximum log level to record.
 * @return 0 on success, -1 on failure.
 */
int logger_init(const char* filename, int level);

/**
 * @brief Logs a message to the file.
 *
 * @param level The log level of the message.
 * @param format The message format string.
 * @param ... Variable arguments for the format string.
 */
void log_message(log_level_t level, const char* format, ...);

/**
 * @brief Closes the log file.
 */
void logger_close();

#ifdef __cplusplus
}
#endif

#endif  // LOGGER_H