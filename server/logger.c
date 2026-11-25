#include "server/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *g_log_fp = NULL;

int logger_init(const char *path) {
    if (!path) return -1;
    g_log_fp = fopen(path, "a");
    if (!g_log_fp) {
        perror("logger_init");
        return -1;
    }
    return 0;
}

void logger_log(log_level_t lv, const char *subject, const char *fmt, ...) {
    FILE *out = g_log_fp ? g_log_fp : stderr;
    const char *lv_str = "INFO ";
    if (lv == LOG_WARN) lv_str = "WARN ";
    else if (lv == LOG_ERROR) lv_str = "ERROR";

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char ts[32];
    if (tm_info) {
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(ts, sizeof(ts), "0000-00-00 00:00:00");
    }

    fprintf(out, "[%s] [%s] %s ", ts, lv_str, subject ? subject : "server");

    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);

    fputc('\n', out);
    fflush(out);
}

void logger_close(void) {
    if (g_log_fp) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
}
