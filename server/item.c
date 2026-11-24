#include "server/item.h"
#include "common/parse.h"
#include "common/protocol.h"
#include "server/auction.h"
#include "server/handler.h"
#include "server/logger.h"
#include "server/room.h"
#include "server/storage.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static item_t g_items[MAX_ITEMS];
static int    g_next_item_id = 1;

const char *item_status_name(item_status_t st) {
    switch (st) {
        case IS_PENDING:    return "PENDING";
        case IS_ON_AUCTION: return "ON_AUCTION";
        case IS_SOLD:       return "SOLD";
        case IS_UNSOLD:     return "UNSOLD";
        case IS_REMOVED:    return "REMOVED";
        default:            return "PENDING";
    }
}

int item_status_parse(const char *s, item_status_t *out) {
    if (!s || !out) return -1;
    if (strcmp(s, "PENDING") == 0)         { *out = IS_PENDING;    return 0; }
    if (strcmp(s, "ON_AUCTION") == 0)      { *out = IS_ON_AUCTION; return 0; }
    if (strcmp(s, "SOLD") == 0)            { *out = IS_SOLD;       return 0; }
    if (strcmp(s, "UNSOLD") == 0)          { *out = IS_UNSOLD;     return 0; }
    if (strcmp(s, "REMOVED") == 0)         { *out = IS_REMOVED;    return 0; }
    return -1;
}

item_t *item_find(int item_id) {
    if (item_id <= 0) return NULL;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (g_items[i].in_use && g_items[i].id == item_id) {
            return &g_items[i];
        }
    }
    return NULL;
}

item_t *item_find_in_room(int item_id, int room_id) {
    item_t *it = item_find(item_id);
    if (!it || it->room_id != room_id) return NULL;
    return it;
}

item_t *item_next_pending(int room_id) {
    item_t *best = NULL;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!g_items[i].in_use) continue;
        if (g_items[i].room_id == room_id && g_items[i].status == IS_PENDING) {
            if (!best || g_items[i].id < best->id) {
                best = &g_items[i];
            }
        }
    }
    return best;
}

int item_pending_count(int room_id) {
    int count = 0;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (g_items[i].in_use &&
            g_items[i].room_id == room_id &&
            g_items[i].status == IS_PENDING) {
            count++;
        }
    }
    return count;
}

static int item_line_cb(char *line, void *ctx) {
    (void)ctx;
    char *tokens[10];
    int n = parse_tokenize(line, tokens, 10);
    if (n != 9) {
        logger_log(LOG_WARN, "server", "STORAGE invalid item line");
        return 0;
    }

    long long id_ll = 0, rid_ll = 0, start_ll = 0, buy_ll = 0, cur_ll = 0;
    item_status_t st;
    if (parse_ll(tokens[0], 1, 1000000000LL, &id_ll) < 0 ||
        parse_ll(tokens[1], 1, 1000000000LL, &rid_ll) < 0 ||
        item_status_parse(tokens[2], &st) < 0 ||
        parse_ll(tokens[3], 1, MAX_PRICE, &start_ll) < 0 ||
        parse_ll(tokens[4], 1, MAX_PRICE, &buy_ll) < 0 ||
        parse_ll(tokens[5], 1, MAX_PRICE, &cur_ll) < 0 ||
        !parse_valid_text(tokens[7], 1, MAX_ITEM_NAME) ||
        !parse_valid_text(tokens[8], 0, MAX_ITEM_DESC)) {
        logger_log(LOG_WARN, "server", "STORAGE malformed item entry");
        return 0;
    }

    if (room_find((int)rid_ll) == NULL) {
        logger_log(LOG_WARN, "server", "STORAGE item %lld references missing room %lld",
                   id_ll, rid_ll);
        return 0;
    }
    if (item_find((int)id_ll) != NULL) return 0;

    int slot = -1;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!g_items[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        logger_log(LOG_WARN, "server", "STORAGE max items reached");
        return 0;
    }

    item_t *it = &g_items[slot];
    memset(it, 0, sizeof(*it));
    it->in_use = 1;
    it->id = (int)id_ll;
    it->room_id = (int)rid_ll;
    it->status = st;
    it->start_price = start_ll;
    it->buy_now_price = buy_ll;
    it->current_price = cur_ll;
    if (strcmp(tokens[6], "-") != 0 && parse_valid_username(tokens[6])) {
        snprintf(it->winner, sizeof(it->winner), "%s", tokens[6]);
    } else {
        it->winner[0] = '\0';
    }
    snprintf(it->name, sizeof(it->name), "%s", tokens[7]);
    snprintf(it->desc, sizeof(it->desc), "%s", tokens[8]);

    if (it->id >= g_next_item_id) {
        g_next_item_id = it->id + 1;
    }
    return 0;
}

static int item_writer_cb(FILE *fp, void *ctx) {
    (void)ctx;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!g_items[i].in_use) continue;
        item_t *it = &g_items[i];
        const char *win = (it->winner[0] != '\0') ? it->winner : "-";
        if (fprintf(fp, "%d %d %s %lld %lld %lld %s \"%s\" \"%s\"\n",
                    it->id, it->room_id, item_status_name(it->status),
                    it->start_price, it->buy_now_price, it->current_price,
                    win, it->name, it->desc) < 0) {
            return -1;
        }
    }
    return 0;
}

int item_load(void) {
    memset(g_items, 0, sizeof(g_items));
    g_next_item_id = 1;
    return storage_read_lines(ITEMS_FILE, item_line_cb, NULL);
}

int item_save(void) {
    if (storage_write_file(ITEMS_FILE, item_writer_cb, NULL) < 0) {
        logger_log(LOG_ERROR, "server", "STORAGE write %s failed: %s",
                   ITEMS_FILE, strerror(errno));
        return -1;
    }
    return 0;
}

int cmd_add_item(session_t *s, int argc, char *argv[]) {
    (void)argc;
    const char *name = argv[0];
    const char *desc = argv[1];
    long long start_price = 0, buy_now_price = 0;

    if (!parse_valid_text(name, 1, MAX_ITEM_NAME)) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid item name");
        return 0;
    }
    if (!parse_valid_text(desc, 0, MAX_ITEM_DESC)) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid description");
        return 0;
    }
    if (parse_ll(argv[2], 1, MAX_PRICE, &start_price) < 0 ||
        parse_ll(argv[3], 1, MAX_PRICE, &buy_now_price) < 0) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid number");
        return 0;
    }
    if (buy_now_price <= start_price) {
        cmd_reply(s, RC_INVALID_PRICE, "Buy-now price must be greater than start price");
        return 0;
    }

    if (item_pending_count(s->room_id) >= MAX_PENDING_PER_ROOM) {
        cmd_reply(s, RC_CONFLICT, "Item queue is full");
        return 0;
    }

    int slot = -1;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!g_items[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        cmd_reply(s, RC_CONFLICT, "Item queue is full");
        return 0;
    }

    item_t *it = &g_items[slot];
    memset(it, 0, sizeof(*it));
    it->in_use = 1;
    it->id = g_next_item_id++;
    it->room_id = s->room_id;
    it->status = IS_PENDING;
    it->start_price = start_price;
    it->buy_now_price = buy_now_price;
    it->current_price = start_price;
    it->winner[0] = '\0';
    snprintf(it->name, sizeof(it->name), "%s", name);
    snprintf(it->desc, sizeof(it->desc), "%s", desc);

    if (item_save() < 0) {
        it->in_use = 0;
        g_next_item_id--;
        cmd_reply(s, RC_INTERNAL, "Internal error");
        return 0;
    }

    cmd_reply(s, RC_CREATED, "Item added %d", it->id);
    session_broadcast_room(s->room_id, NULL, "NOTIFY %s %d \"%s\"",
                           EVT_ITEM_ADDED, it->id, it->name);

    room_t *r = room_find(s->room_id);
    if (r && r->current_item_id == 0) {
        auction_start_next(r, time(NULL));
    }
    return 0;
}

int cmd_remove_item(session_t *s, int argc, char *argv[]) {
    (void)argc;
    long long item_id_ll = 0;
    if (parse_ll(argv[0], 1, 1000000000LL, &item_id_ll) < 0) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid number");
        return 0;
    }

    item_t *it = item_find_in_room((int)item_id_ll, s->room_id);
    if (!it) {
        cmd_reply(s, RC_NOT_FOUND, "Item not found");
        return 0;
    }
    if (it->status != IS_PENDING) {
        cmd_reply(s, RC_CONFLICT, "Item is not pending");
        return 0;
    }

    it->status = IS_REMOVED;
    item_save();

    cmd_reply(s, RC_OK, "Item removed %d", it->id);
    session_broadcast_room(s->room_id, NULL, "NOTIFY %s %d", EVT_ITEM_REMOVED, it->id);
    return 0;
}

int cmd_list_items(session_t *s, int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    item_t *list[MAX_ITEMS];
    int count = 0;

    /* Vật phẩm ON_AUCTION đứng đầu, tiếp theo các PENDING theo id tăng dần */
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (g_items[i].in_use &&
            g_items[i].room_id == s->room_id &&
            g_items[i].status == IS_ON_AUCTION) {
            list[count++] = &g_items[i];
        }
    }

    item_t *pend[MAX_ITEMS];
    int pcount = 0;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (g_items[i].in_use &&
            g_items[i].room_id == s->room_id &&
            g_items[i].status == IS_PENDING) {
            pend[pcount++] = &g_items[i];
        }
    }
    for (int i = 0; i < pcount - 1; i++) {
        for (int j = i + 1; j < pcount; j++) {
            if (pend[i]->id > pend[j]->id) {
                item_t *tmp = pend[i];
                pend[i] = pend[j];
                pend[j] = tmp;
            }
        }
    }
    for (int i = 0; i < pcount; i++) {
        list[count++] = pend[i];
    }

    char buf[MAX_RESP_LINE - 64];
    size_t pos = 0;
    int n = snprintf(buf, sizeof(buf), "ITEMS %d", count);
    if (n > 0) pos = (size_t)n;

    for (int i = 0; i < count; i++) {
        item_t *it = list[i];
        const char *win = (it->winner[0] != '\0') ? it->winner : "-";
        n = snprintf(buf + pos, sizeof(buf) - pos,
                     "; %d %s \"%s\" \"%s\" %lld %lld %lld %s",
                     it->id, item_status_name(it->status),
                     it->name, it->desc,
                     it->start_price, it->buy_now_price, it->current_price, win);
        if (n < 0 || (size_t)n >= sizeof(buf) - pos) break;
        pos += (size_t)n;
    }

    cmd_reply(s, RC_OK, "%s", buf);
    return 0;
}
