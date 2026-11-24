#include "common/parse.h"
#include "common/protocol.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int parse_tokenize(char *line, char *tokens[], int max_tokens) {
    if (!line || !tokens || max_tokens <= 0) return -1;

    int count = 0;
    char *p = line;

    while (*p != '\0') {
        while (*p == ' ') p++;
        if (*p == '\0') break;

        /* Kiểm tra ký tự điều khiển (< 0x20) */
        for (char *c = p; *c != '\0'; c++) {
            if ((unsigned char)*c < 0x20) return -1;
        }

        if (*p == '"') {
            p++; /* bỏ '"' mở */
            char *start = p;
            while (*p != '\0' && *p != '"') {
                if ((unsigned char)*p < 0x20) return -1;
                p++;
            }
            if (*p != '"') return -1; /* thiếu '"' đóng */
            *p = '\0';
            p++;
            if (*p != '\0' && *p != ' ') return -1; /* sau '"' đóng phải là space hoặc hết dòng */
            if (count >= max_tokens) return -2;
            tokens[count++] = start;
        } else {
            char *start = p;
            while (*p != '\0' && *p != ' ') {
                if (*p == '"' || (unsigned char)*p < 0x20) return -1;
                p++;
            }
            if (*p == ' ') {
                *p = '\0';
                p++;
            }
            if (count >= max_tokens) return -2;
            tokens[count++] = start;
        }
    }
    return count;
}

int parse_ll(const char *s, long long min, long long max, long long *out) {
    if (!s || *s == '\0' || !out) return -1;
    for (const char *p = s; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') return -1;
    }
    errno = 0;
    char *end = NULL;
    long long val = strtoll(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0') return -1;
    if (val < min || val > max) return -1;
    *out = val;
    return 0;
}

int parse_valid_username(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len < MIN_USERNAME || len > MAX_USERNAME) return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            return 0;
        }
    }
    return 1;
}

int parse_valid_password(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len < MIN_PASSWORD || len > MAX_PASSWORD) return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c <= 0x20 || c >= 0x7f || c == '"') return 0;
    }
    return 1;
}

int parse_valid_text(const char *s, size_t min_len, size_t max_len) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len < min_len || len > max_len) return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == '"' || c == 0x7f) return 0;
    }
    return 1;
}

int parse_split_records(char *line, char *records[], int max_records) {
    if (!line || !records || max_records <= 0) return -1;
    int count = 0;
    int in_quote = 0;
    char *start = line;

    for (char *p = line; *p != '\0'; p++) {
        if (*p == '"') {
            in_quote = !in_quote;
        } else if (!in_quote && p[0] == ';' && p[1] == ' ') {
            if (count >= max_records) return -1;
            *p = '\0';
            records[count++] = start;
            p++; /* bước qua dấu cách sau ';' */
            start = p + 1;
        }
    }
    if (in_quote) return -1;
    if (*start != '\0') {
        if (count >= max_records) return -1;
        records[count++] = start;
    }
    return count;
}
