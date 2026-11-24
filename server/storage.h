#ifndef SERVER_STORAGE_H
#define SERVER_STORAGE_H

#include <stdio.h>

typedef int (*storage_line_cb)(char *line, void *ctx);
int storage_read_lines(const char *path, storage_line_cb cb, void *ctx);

typedef int (*storage_writer)(FILE *fp, void *ctx);
int storage_write_file(const char *path, storage_writer writer, void *ctx);

#endif /* SERVER_STORAGE_H */
