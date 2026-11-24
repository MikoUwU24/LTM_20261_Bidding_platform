#ifndef SERVER_ROOM_H
#define SERVER_ROOM_H

#include <time.h>
#include "common/protocol.h"
#include "server/session.h"

typedef struct {
    int    in_use;
    int    id;
    char   owner[MAX_USERNAME + 1];
    char   name[MAX_ROOM_NAME + 1];
    int    current_item_id;
    time_t deadline;
    int    countdown_stage;
} room_t;

int     room_load(void);
int     room_save(void);
room_t *room_find(int room_id);
room_t *room_at(int index);
int     room_capacity(void);
int     room_member_count(int room_id);
void    room_leave(session_t *s);

int cmd_create_room(session_t *s, int argc, char *argv[]);
int cmd_list_rooms (session_t *s, int argc, char *argv[]);
int cmd_join_room  (session_t *s, int argc, char *argv[]);
int cmd_leave_room (session_t *s, int argc, char *argv[]);

#endif /* SERVER_ROOM_H */
