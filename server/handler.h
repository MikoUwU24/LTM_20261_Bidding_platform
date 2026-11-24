#ifndef SERVER_HANDLER_H
#define SERVER_HANDLER_H

#include "server/session.h"

enum {
    G_NONE        = 0,
    G_NOT_AUTH    = 1 << 0,
    G_AUTH        = 1 << 1,
    G_IN_ROOM     = 1 << 2,
    G_NOT_IN_ROOM = 1 << 3,
    G_OWNER       = 1 << 4,
    G_NOT_OWNER   = 1 << 5
};

typedef int (*cmd_fn)(session_t *s, int argc, char *argv[]);

typedef struct {
    const char *name;
    int         argc;
    int         guards;
    cmd_fn      fn;
} command_t;

void handler_process_line(session_t *s, char *line);
void cmd_reply(session_t *s, int code, const char *fmt, ...);

int cmd_register(session_t *s, int argc, char *argv[]);
int cmd_login(session_t *s, int argc, char *argv[]);
int cmd_logout(session_t *s, int argc, char *argv[]);

#endif /* SERVER_HANDLER_H */
