#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H

typedef enum {
    LOG_INFO = 0,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

int  logger_init(const char *path);
void logger_log(log_level_t lv, const char *subject, const char *fmt, ...);
void logger_close(void);

#endif /* SERVER_LOGGER_H */
