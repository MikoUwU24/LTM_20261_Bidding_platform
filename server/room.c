#include "server/room.h"
#include "common/parse.h"
#include "common/protocol.h"
#include "server/auction.h"
#include "server/handler.h"
#include "server/item.h"
#include "server/logger.h"
#include "server/storage.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static room_t g_rooms[MAX_ROOMS];
static int    g_next_room_id = 1;

room_t *room_find(int room_id) {
    if (room_id <= 0) return NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (g_rooms[i].in_use && g_rooms[i].id == room_id) {
            return &g_rooms[i];
        }
    }
    return NULL;
}

room_t *room_at(int index) {
    if (index < 0 || index >= MAX_ROOMS) return NULL;
    if (!g_rooms[index].in_use) return NULL;
    return &g_rooms[index];
}

int room_capacity(void) {
    return MAX_ROOMS;
}

int room_member_count(int room_id) {
    if (room_id <= 0) return 0;
    int count = 0;
    for (int i = 0; i < session_capacity(); i++) {
        session_t *s = session_at(i);
        if (s && s->state == SS_IN_ROOM && s->room_id == room_id) {
            count++;
        }
    }
    return count;
}

static int room_line_cb(char *line, void *ctx) {
    (void)ctx;
    char *tokens[4];
    int n = parse_tokenize(line, tokens, 4);
    if (n != 3) {
        logger_log(LOG_WARN, "server", "STORAGE invalid room line");
        return 0;
    }

    long long id_ll = 0;
    if (parse_ll(tokens[0], 1, 1000000000LL, &id_ll) < 0 ||
        !parse_valid_username(tokens[1]) ||
        !parse_valid_text(tokens[2], 1, MAX_ROOM_NAME)) {
        logger_log(LOG_WARN, "server", "STORAGE malformed room entry");
        return 0;
    }
    if (room_find((int)id_ll) != NULL) return 0;

    int slot = -1;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!g_rooms[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        logger_log(LOG_WARN, "server", "STORAGE max rooms reached");
        return 0;
    }

    room_t *r = &g_rooms[slot];
    memset(r, 0, sizeof(*r));
    r->in_use = 1;
    r->id = (int)id_ll;
    snprintf(r->owner, sizeof(r->owner), "%s", tokens[1]);
    snprintf(r->name, sizeof(r->name), "%s", tokens[2]);
    if (r->id >= g_next_room_id) {
        g_next_room_id = r->id + 1;
    }
    return 0;
}

static int room_writer_cb(FILE *fp, void *ctx) {
    (void)ctx;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!g_rooms[i].in_use) continue;
        if (fprintf(fp, "%d %s \"%s\"\n",
                    g_rooms[i].id,
                    g_rooms[i].owner,
                    g_rooms[i].name) < 0) {
            return -1;
        }
    }
    return 0;
}

int room_load(void) {
    memset(g_rooms, 0, sizeof(g_rooms));
    g_next_room_id = 1;
    return storage_read_lines(ROOMS_FILE, room_line_cb, NULL);
}

int room_save(void) {
    if (storage_write_file(ROOMS_FILE, room_writer_cb, NULL) < 0) {
        logger_log(LOG_ERROR, "server", "STORAGE write %s failed: %s",
                   ROOMS_FILE, strerror(errno));
        return -1;
    }
    return 0;
}

void room_leave(session_t *s) {
    if (!s || s->state != SS_IN_ROOM || s->room_id <= 0) return;
    int rid = s->room_id;
    s->room_id = 0;
    s->state = SS_AUTH;
    session_broadcast_room(rid, s, "NOTIFY %s %s", EVT_USER_LEFT, s->username);
}

int cmd_create_room(session_t *s, int argc, char *argv[]) {
    (void)argc;
    const char *name = argv[0];
    if (!parse_valid_text(name, 1, MAX_ROOM_NAME)) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid room name");
        return 0;
    }

    int slot = -1;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!g_rooms[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        cmd_reply(s, RC_INTERNAL, "Server storage full");
        return 0;
    }

    room_t *r = &g_rooms[slot];
    memset(r, 0, sizeof(*r));
    r->in_use = 1;
    r->id = g_next_room_id++;
    snprintf(r->owner, sizeof(r->owner), "%s", s->username);
    snprintf(r->name, sizeof(r->name), "%s", name);

    room_save();

    s->room_id = r->id;
    s->state = SS_IN_ROOM;
    cmd_reply(s, RC_CREATED, "Room created %d", r->id);
    return 0;
}

int cmd_list_rooms(session_t *s, int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Thu thập và sắp xếp theo room_id tăng dần */
    room_t *list[MAX_ROOMS];
    int count = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (g_rooms[i].in_use) {
            list[count++] = &g_rooms[i];
        }
    }
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (list[i]->id > list[j]->id) {
                room_t *tmp = list[i];
                list[i] = list[j];
                list[j] = tmp;
            }
        }
    }

    char buf[MAX_RESP_LINE - 64];
    size_t pos = 0;
    int n = snprintf(buf, sizeof(buf), "ROOMS %d", count);
    if (n > 0) pos = (size_t)n;

    for (int i = 0; i < count; i++) {
        room_t *r = list[i];
        int members = room_member_count(r->id);
        int pending = item_pending_count(r->id);
        char cur_item[32];
        if (r->current_item_id > 0) {
            snprintf(cur_item, sizeof(cur_item), "%d", r->current_item_id);
        } else {
            snprintf(cur_item, sizeof(cur_item), "-");
        }

        n = snprintf(buf + pos, sizeof(buf) - pos,
                     "; %d %s \"%s\" %d %s %d",
                     r->id, r->owner, r->name, members, cur_item, pending);
        if (n < 0 || (size_t)n >= sizeof(buf) - pos) break;
        pos += (size_t)n;
    }

    cmd_reply(s, RC_OK, "%s", buf);
    return 0;
}

int cmd_join_room(session_t *s, int argc, char *argv[]) {
    (void)argc;
    long long rid_ll = 0;
    if (parse_ll(argv[0], 1, 1000000000LL, &rid_ll) < 0) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid number");
        return 0;
    }

    room_t *r = room_find((int)rid_ll);
    if (!r) {
        cmd_reply(s, RC_NOT_FOUND, "Room not found");
        return 0;
    }

    s->room_id = r->id;
    s->state = SS_IN_ROOM;

    cmd_reply(s, RC_OK, "Joined room %d \"%s\"", r->id, r->name);
    session_broadcast_room(r->id, s, "NOTIFY %s %s", EVT_USER_JOINED, s->username);
    auction_send_state(s, r);
    return 0;
}

int cmd_leave_room(session_t *s, int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int rid = s->room_id;
    cmd_reply(s, RC_OK, "Left room %d", rid);
    room_leave(s);
    return 0;
}
