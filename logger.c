#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static FILE* log_file = NULL;
static int current_log_level = LOG_LEVEL_ERROR;
static const char* level_strings[] = {"ERROR", "WARN", "INFO", "DEBUG"};

int logger_init(const char* filename, int level) {
    if (filename) {
        log_file = fopen(filename, "a");
        if (log_file == NULL) {
            perror("Failed to open log file");
            return -1;
        }
    } else {
        log_file = stdout;
    }
    current_log_level = level;
    log_message(LOG_LEVEL_INFO, "Logger initialized.");
    return 0;
}

void log_message(log_level_t level, const char* format, ...) {
    if (level > current_log_level) {
        return;
    }

    // Get current time
    time_t now = time(NULL);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Print log prefix
    fprintf(log_file, "%s [%s] - ", time_buf, level_strings[level]);

    // Print user message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    // Newline and flush
    fprintf(log_file, "\n");
    fflush(log_file);
}

void logger_close() {
    if (log_file != NULL && log_file != stdout) {
        log_message(LOG_LEVEL_INFO, "Closing log file.");
        fclose(log_file);
        log_file = NULL;
    }
}
