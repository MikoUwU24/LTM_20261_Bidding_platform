#ifndef SERVER_AUCTION_H
#define SERVER_AUCTION_H

#include <time.h>
#include "server/session.h"
#include "server/room.h"

void auction_start_next(room_t *r, time_t now);
void auction_finish(room_t *r, time_t now);
void auction_tick(time_t now);
long auction_next_timeout(time_t now);
void auction_send_state(session_t *s, const room_t *r);
void auction_resume_all(time_t now);

int cmd_bid    (session_t *s, int argc, char *argv[]);
int cmd_buy_now(session_t *s, int argc, char *argv[]);

#endif /* SERVER_AUCTION_H */
