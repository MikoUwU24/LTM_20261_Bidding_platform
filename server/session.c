#include "server/session.h"
#include "server/logger.h"
#include "server/room.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static session_t g_sessions[MAX_CLIENTS];

void session_init_all(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_sessions[i].fd = -1;
    }
}

session_t *session_create(int fd, const char *addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_sessions[i].in_use) {
            session_t *s = &g_sessions[i];
            memset(s, 0, sizeof(*s));
            s->in_use = 1;
            s->fd = fd;
            snprintf(s->addr, sizeof(s->addr), "%s", addr ? addr : "0.0.0.0:0");
            s->state = SS_CONNECTED;
            stream_recv_init(&s->rbuf);
            stream_send_init(&s->sbuf);
            return s;
        }
    }
    return NULL;
}

session_t *session_at(int index) {
    if (index < 0 || index >= MAX_CLIENTS) return NULL;
    if (!g_sessions[index].in_use) return NULL;
    return &g_sessions[index];
}

int session_capacity(void) {
    return MAX_CLIENTS;
}

session_t *session_find_by_user(const char *username) {
    if (!username || username[0] == '\0') return NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_sessions[i].in_use &&
            g_sessions[i].state != SS_CONNECTED &&
            strcmp(g_sessions[i].username, username) == 0) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

const char *session_subject(const session_t *s) {
    if (!s) return "server";
    if (s->state != SS_CONNECTED && s->username[0] != '\0') {
        return s->username;
    }
    return s->addr;
}

static void session_enqueue_formatted(session_t *s, const char *fmt, va_list ap) {
    if (!s || !s->in_use) return;
    char line[MAX_RESP_LINE];
    int n = vsnprintf(line, sizeof(line) - 2, fmt, ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(line) - 2) {
        n = (int)(sizeof(line) - 3);
    }
    line[n] = '\r';
    line[n + 1] = '\n';
    line[n + 2] = '\0';

    if (stream_send_append(&s->sbuf, line, (size_t)(n + 2)) < 0) {
        logger_log(LOG_WARN, session_subject(s), "IO send buffer overflow");
        s->closing = 1;
    }
}

void session_send(session_t *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    session_enqueue_formatted(s, fmt, ap);
    va_end(ap);
}

void session_broadcast_room(int room_id, const session_t *exclude, const char *fmt, ...) {
    if (room_id <= 0) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        session_t *s = &g_sessions[i];
        if (!s->in_use || s == exclude) continue;
        if (s->state == SS_IN_ROOM && s->room_id == room_id) {
            va_list ap;
            va_start(ap, fmt);
            session_enqueue_formatted(s, fmt, ap);
            va_end(ap);
        }
    }
}

void session_close(session_t *s, const char *reason) {
    if (!s || !s->in_use) return;
    if (s->state == SS_IN_ROOM && s->room_id > 0) {
        room_leave(s);
    }
    logger_log(LOG_INFO, session_subject(s), "DISCONNECT %s", reason ? reason : "closed");
    if (s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
    s->in_use = 0;
}
