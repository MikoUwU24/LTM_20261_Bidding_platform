#ifndef SERVER_ITEM_H
#define SERVER_ITEM_H

#include "common/protocol.h"
#include "server/session.h"

typedef enum {
    IS_PENDING = 0,
    IS_ON_AUCTION,
    IS_SOLD,
    IS_UNSOLD,
    IS_REMOVED
} item_status_t;

typedef struct {
    int           in_use;
    int           id;
    int           room_id;
    item_status_t status;
    long long     start_price;
    long long     buy_now_price;
    long long     current_price;
    char          winner[MAX_USERNAME + 1];
    char          name[MAX_ITEM_NAME + 1];
    char          desc[MAX_ITEM_DESC + 1];
} item_t;

int         item_load(void);
int         item_save(void);
item_t     *item_find(int item_id);
item_t     *item_find_in_room(int item_id, int room_id);
item_t     *item_next_pending(int room_id);
int         item_pending_count(int room_id);
const char *item_status_name(item_status_t st);
int         item_status_parse(const char *s, item_status_t *out);

int cmd_add_item   (session_t *s, int argc, char *argv[]);
int cmd_remove_item(session_t *s, int argc, char *argv[]);
int cmd_list_items (session_t *s, int argc, char *argv[]);

#endif /* SERVER_ITEM_H */
