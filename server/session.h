#ifndef SERVER_SESSION_H
#define SERVER_SESSION_H

#include "common/protocol.h"
#include "common/stream.h"

typedef enum {
    SS_CONNECTED = 0,
    SS_AUTH,
    SS_IN_ROOM
} session_state_t;

typedef struct session {
    int             in_use;
    int             fd;
    char            addr[32];
    session_state_t state;
    char            username[MAX_USERNAME + 1];
    int             room_id;
    int             closing;
    int             replied;
    char            cur_cmd[32];
    char            cur_log[160];
    recv_buffer_t   rbuf;
    send_buffer_t   sbuf;
} session_t;

void        session_init_all(void);
session_t  *session_create(int fd, const char *addr);
session_t  *session_at(int index);
int         session_capacity(void);
session_t  *session_find_by_user(const char *username);
void        session_send(session_t *s, const char *fmt, ...);
void        session_broadcast_room(int room_id, const session_t *exclude, const char *fmt, ...);
const char *session_subject(const session_t *s);
void        session_close(session_t *s, const char *reason);

#endif /* SERVER_SESSION_H */
