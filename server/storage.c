#include "server/storage.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

int storage_read_lines(const char *path, storage_line_cb cb, void *ctx) {
    if (!path || !cb) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (errno == ENOENT) return 0;
        return -1;
    }

    char line[4096];
    int rc = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;
        if (cb(line, ctx) < 0) {
            rc = -1;
            break;
        }
    }
    fclose(fp);
    return rc;
}

int storage_write_file(const char *path, storage_writer writer, void *ctx) {
    if (!path || !writer) return -1;
    char tmp_path[512];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) return -1;

    FILE *fp = fopen(tmp_path, "w");
    if (!fp) return -1;

    if (writer(fp, ctx) < 0 || fflush(fp) != 0) {
        fclose(fp);
        remove(tmp_path);
        return -1;
    }
    if (fclose(fp) != 0) {
        remove(tmp_path);
        return -1;
    }
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return -1;
    }
    return 0;
}
