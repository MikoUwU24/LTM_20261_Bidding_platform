#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

#include <sys/select.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 5500
#endif

#ifndef BACKLOG
#define BACKLOG 16
#endif

#ifndef MAX_LINE
#define MAX_LINE 4096
#endif

#ifndef MAX_RESP_LINE
#define MAX_RESP_LINE 16384
#endif

#ifndef RECV_BUF_SIZE
#define RECV_BUF_SIZE MAX_RESP_LINE
#endif

#ifndef SEND_BUF_SIZE
#define SEND_BUF_SIZE 65536
#endif

#ifndef MAX_CLIENTS
#define MAX_CLIENTS (FD_SETSIZE - 8)
#endif

#ifndef MAX_TOKENS
#define MAX_TOKENS 8
#endif

#ifndef MIN_USERNAME
#define MIN_USERNAME 3
#endif

#ifndef MAX_USERNAME
#define MAX_USERNAME 32
#endif

#ifndef MIN_PASSWORD
#define MIN_PASSWORD 4
#endif

#ifndef MAX_PASSWORD
#define MAX_PASSWORD 64
#endif

#ifndef MAX_ROOM_NAME
#define MAX_ROOM_NAME 64
#endif

#ifndef MAX_ITEM_NAME
#define MAX_ITEM_NAME 64
#endif

#ifndef MAX_ITEM_DESC
#define MAX_ITEM_DESC 256
#endif

#ifndef MAX_ACCOUNTS
#define MAX_ACCOUNTS 1024
#endif

#ifndef MAX_ROOMS
#define MAX_ROOMS 64
#endif

#ifndef MAX_ITEMS
#define MAX_ITEMS 2048
#endif

#ifndef MAX_PENDING_PER_ROOM
#define MAX_PENDING_PER_ROOM 32
#endif

#ifndef MAX_PRICE
#define MAX_PRICE 1000000000000LL
#endif

#ifndef BID_STEP
#define BID_STEP 10000
#endif

#ifndef AUCTION_IDLE_SEC
#define AUCTION_IDLE_SEC 180
#endif

#ifndef COUNTDOWN_INTERVAL_SEC
#define COUNTDOWN_INTERVAL_SEC 10
#endif

#ifndef COUNTDOWN_TIMES
#define COUNTDOWN_TIMES 3
#endif

#ifndef SALT_LEN
#define SALT_LEN 16
#endif

#ifndef HASH_ROUNDS
#define HASH_ROUNDS 1000
#endif

#ifndef DATA_DIR
#define DATA_DIR "data"
#endif

#ifndef LOG_FILE
#define LOG_FILE "logs/server.log"
#endif

#ifndef ACCOUNTS_FILE
#define ACCOUNTS_FILE "data/accounts.txt"
#endif

#ifndef ROOMS_FILE
#define ROOMS_FILE "data/rooms.txt"
#endif

#ifndef ITEMS_FILE
#define ITEMS_FILE "data/items.txt"
#endif

/* Response codes */
enum {
    RC_OK            = 200,
    RC_CREATED       = 201,
    RC_BAD_REQUEST   = 400,
    RC_UNAUTHORIZED  = 401,
    RC_FORBIDDEN     = 403,
    RC_NOT_FOUND     = 404,
    RC_CONFLICT      = 409,
    RC_INVALID_PRICE = 422,
    RC_INTERNAL      = 500,
    RC_BUSY          = 503
};

/* Commands */
#define CMD_REGISTER    "REGISTER"
#define CMD_LOGIN       "LOGIN"
#define CMD_LOGOUT      "LOGOUT"
#define CMD_CREATE_ROOM "CREATE_ROOM"
#define CMD_LIST_ROOMS  "LIST_ROOMS"
#define CMD_JOIN_ROOM   "JOIN_ROOM"
#define CMD_LEAVE_ROOM  "LEAVE_ROOM"
#define CMD_LIST_ITEMS  "LIST_ITEMS"
#define CMD_ADD_ITEM    "ADD_ITEM"
#define CMD_REMOVE_ITEM "REMOVE_ITEM"
#define CMD_BID         "BID"
#define CMD_BUY_NOW     "BUY_NOW"

/* Notification events */
#define EVT_USER_JOINED   "USER_JOINED"
#define EVT_USER_LEFT     "USER_LEFT"
#define EVT_ITEM_ADDED    "ITEM_ADDED"
#define EVT_ITEM_REMOVED  "ITEM_REMOVED"
#define EVT_AUCTION_START "AUCTION_START"
#define EVT_NEW_BID       "NEW_BID"
#define EVT_COUNTDOWN     "COUNTDOWN"
#define EVT_SOLD          "SOLD"
#define EVT_UNSOLD        "UNSOLD"

#endif /* COMMON_PROTOCOL_H */
