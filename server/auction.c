#include "server/auction.h"
#include "common/parse.h"
#include "common/protocol.h"
#include "server/handler.h"
#include "server/item.h"
#include "server/logger.h"
#include <stdio.h>
#include <string.h>

void auction_start_next(room_t *r, time_t now) {
    if (!r || !r->in_use) return;

    item_t *next = item_next_pending(r->id);
    if (!next) {
        r->current_item_id = 0;
        r->deadline = 0;
        r->countdown_stage = 0;
        return;
    }

    next->status = IS_ON_AUCTION;
    next->current_price = next->start_price;
    next->winner[0] = '\0';
    item_save();

    r->current_item_id = next->id;
    r->countdown_stage = 0;
    r->deadline = now + AUCTION_IDLE_SEC;

    char subj[32];
    snprintf(subj, sizeof(subj), "room=%d", r->id);
    logger_log(LOG_INFO, subj, "AUCTION_START item=%d", next->id);

    session_broadcast_room(r->id, NULL, "NOTIFY %s %d \"%s\" \"%s\" %lld %lld",
                           EVT_AUCTION_START, next->id, next->name, next->desc,
                           next->start_price, next->buy_now_price);
}

void auction_finish(room_t *r, time_t now) {
    if (!r || !r->in_use || r->current_item_id <= 0) return;

    item_t *it = item_find_in_room(r->current_item_id, r->id);
    char subj[32];
    snprintf(subj, sizeof(subj), "room=%d", r->id);

    if (it && it->status == IS_ON_AUCTION) {
        if (it->winner[0] != '\0') {
            it->status = IS_SOLD;
            item_save();
            logger_log(LOG_INFO, subj, "SOLD item=%d to=%s price=%lld",
                       it->id, it->winner, it->current_price);
            session_broadcast_room(r->id, NULL, "NOTIFY %s %d %s %lld",
                                   EVT_SOLD, it->id, it->winner, it->current_price);
        } else {
            it->status = IS_UNSOLD;
            item_save();
            logger_log(LOG_INFO, subj, "UNSOLD item=%d", it->id);
            session_broadcast_room(r->id, NULL, "NOTIFY %s %d", EVT_UNSOLD, it->id);
        }
    }

    r->current_item_id = 0;
    r->deadline = 0;
    r->countdown_stage = 0;
    auction_start_next(r, now);
}

void auction_tick(time_t now) {
    for (int i = 0; i < room_capacity(); i++) {
        room_t *r = room_at(i);
        if (!r || r->current_item_id <= 0 || r->deadline == 0) continue;
        if (r->deadline > now) continue;

        item_t *it = item_find_in_room(r->current_item_id, r->id);
        if (!it || it->status != IS_ON_AUCTION) {
            r->current_item_id = 0;
            r->deadline = 0;
            r->countdown_stage = 0;
            auction_start_next(r, now);
            continue;
        }

        if (r->countdown_stage < COUNTDOWN_TIMES) {
            r->countdown_stage++;
            r->deadline = now + COUNTDOWN_INTERVAL_SEC;

            char subj[32];
            snprintf(subj, sizeof(subj), "room=%d", r->id);
            logger_log(LOG_INFO, subj, "COUNTDOWN item=%d stage=%d price=%lld",
                       it->id, r->countdown_stage, it->current_price);

            session_broadcast_room(r->id, NULL, "NOTIFY %s %d %d %lld",
                                   EVT_COUNTDOWN, it->id, r->countdown_stage,
                                   it->current_price);
        } else {
            auction_finish(r, now);
        }
    }
}

long auction_next_timeout(time_t now) {
    long best = -1;
    for (int i = 0; i < room_capacity(); i++) {
        room_t *r = room_at(i);
        if (!r || r->current_item_id <= 0 || r->deadline == 0) continue;
        long diff = (long)(r->deadline - now);
        if (diff < 0) diff = 0;
        if (best < 0 || diff < best) {
            best = diff;
        }
    }
    return best;
}

void auction_send_state(session_t *s, const room_t *r) {
    if (!s || !r || r->current_item_id <= 0) return;
    item_t *it = item_find_in_room(r->current_item_id, r->id);
    if (!it || it->status != IS_ON_AUCTION) return;

    session_send(s, "NOTIFY %s %d \"%s\" \"%s\" %lld %lld",
                 EVT_AUCTION_START, it->id, it->name, it->desc,
                 it->start_price, it->buy_now_price);
    if (it->winner[0] != '\0') {
        session_send(s, "NOTIFY %s %d %s %lld",
                     EVT_NEW_BID, it->id, it->winner, it->current_price);
    }
}

void auction_resume_all(time_t now) {
    int changed = 0;
    for (int i = 0; i < room_capacity(); i++) {
        room_t *r = room_at(i);
        if (!r) continue;
        r->current_item_id = 0;
        r->deadline = 0;
        r->countdown_stage = 0;

        /* Tìm vật phẩm ON_AUCTION có id nhỏ nhất trong phòng (spec 04 §2.3) */
        item_t *active = NULL;
        for (int id = 1; id < MAX_ITEMS * 4; id++) {
            item_t *it = item_find_in_room(id, r->id);
            if (!it) continue;
            if (it->status == IS_ON_AUCTION) {
                if (!active) {
                    active = it;
                } else {
                    logger_log(LOG_WARN, "server",
                               "STORAGE room %d has multiple ON_AUCTION; demoting item %d",
                               r->id, it->id);
                    it->status = IS_PENDING;
                    changed = 1;
                }
            }
        }

        if (active) {
            r->current_item_id = active->id;
            r->countdown_stage = 0;
            r->deadline = now + AUCTION_IDLE_SEC;
        } else {
            auction_start_next(r, now);
        }
    }
    if (changed) {
        item_save();
    }
}

static void do_buy_now(session_t *s, room_t *r, item_t *it, time_t now) {
    it->current_price = it->buy_now_price;
    snprintf(it->winner, sizeof(it->winner), "%s", s->username);
    it->status = IS_SOLD;
    item_save();

    cmd_reply(s, RC_OK, "Bought item %d price %lld", it->id, it->buy_now_price);

    char subj[32];
    snprintf(subj, sizeof(subj), "room=%d", r->id);
    logger_log(LOG_INFO, subj, "SOLD item=%d to=%s price=%lld",
               it->id, it->winner, it->current_price);

    session_broadcast_room(r->id, NULL, "NOTIFY %s %d %s %lld",
                           EVT_SOLD, it->id, it->winner, it->current_price);

    r->current_item_id = 0;
    r->deadline = 0;
    r->countdown_stage = 0;
    auction_start_next(r, now);
}

int cmd_bid(session_t *s, int argc, char *argv[]) {
    (void)argc;
    long long item_id_ll = 0, price_ll = 0;
    if (parse_ll(argv[0], 1, 1000000000LL, &item_id_ll) < 0 ||
        parse_ll(argv[1], 1, MAX_PRICE, &price_ll) < 0) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid number");
        return 0;
    }

    item_t *it = item_find_in_room((int)item_id_ll, s->room_id);
    if (!it) {
        cmd_reply(s, RC_NOT_FOUND, "Item not found");
        return 0;
    }
    if (it->status != IS_ON_AUCTION) {
        cmd_reply(s, RC_CONFLICT, "Item is not on auction");
        return 0;
    }

    long long min_bid = it->current_price + BID_STEP;
    if (price_ll < min_bid) {
        cmd_reply(s, RC_INVALID_PRICE, "Bid must be at least %lld", min_bid);
        return 0;
    }

    room_t *r = room_find(s->room_id);
    if (!r) {
        cmd_reply(s, RC_INTERNAL, "Internal error");
        return 0;
    }

    time_t now = time(NULL);
    if (price_ll >= it->buy_now_price) {
        do_buy_now(s, r, it, now);
        return 0;
    }

    it->current_price = price_ll;
    snprintf(it->winner, sizeof(it->winner), "%s", s->username);
    item_save();

    r->countdown_stage = 0;
    r->deadline = now + AUCTION_IDLE_SEC;

    cmd_reply(s, RC_OK, "Bid accepted");
    session_broadcast_room(r->id, NULL, "NOTIFY %s %d %s %lld",
                           EVT_NEW_BID, it->id, s->username, price_ll);
    return 0;
}

int cmd_buy_now(session_t *s, int argc, char *argv[]) {
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
    if (it->status != IS_ON_AUCTION) {
        cmd_reply(s, RC_CONFLICT, "Item is not on auction");
        return 0;
    }

    room_t *r = room_find(s->room_id);
    if (!r) {
        cmd_reply(s, RC_INTERNAL, "Internal error");
        return 0;
    }

    do_buy_now(s, r, it, time(NULL));
    return 0;
}
