#!/usr/bin/env bash
set -u
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

start_server || exit 1
client_open A
client_open B

login_as A alice pass1
login_as B bob pass1

client_send A "CREATE_ROOM \"Do co\""
expect_line A "^201 Room created 1$"
client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Do co\"$"
expect_line A "^NOTIFY USER_JOINED bob$"

# AT-15: ADD_ITEM khi phòng rỗng -> 201 trước, rồi ITEM_ADDED, rồi AUCTION_START
client_send A "ADD_ITEM \"Binh gom\" \"Gom co\" 500000 2000000"
if expect_line A "^201 Item added 1$" &&
   expect_line A "^NOTIFY ITEM_ADDED 1 \"Binh gom\"$" &&
   expect_line A "^NOTIFY AUCTION_START 1 \"Binh gom\" \"Gom co\" 500000 2000000$" &&
   expect_line B "^NOTIFY ITEM_ADDED 1 \"Binh gom\"$" &&
   expect_line B "^NOTIFY AUCTION_START 1 \"Binh gom\" \"Gom co\" 500000 2000000$"; then
    pass_at "AT-15"
else
    fail_at "AT-15" "ADD_ITEM response/notification order mismatch"
fi

# AT-16: Kiểm tra lỗi giá và tham số ADD_ITEM
r=0
client_send A "ADD_ITEM \"a\" \"b\" 500 500"
expect_line A "^422 Buy-now price must be greater than start price$" || r=1
client_send A "ADD_ITEM \"a\" \"b\" -1 10"
expect_line A "^400 Invalid number$" || r=1
client_send A "ADD_ITEM \"a\" \"b\" 1x 10"
expect_line A "^400 Invalid number$" || r=1
client_send A "ADD_ITEM \"a\" b 1"
expect_line A "^400 Wrong number of arguments$" || r=1
if (( r == 0 )); then
    pass_at "AT-16"
else
    fail_at "AT-16" "ADD_ITEM validation failed"
fi

# AT-17: Hàng đợi FIFO và LIST_ITEMS
client_send A "ADD_ITEM \"Lo hoa\" \"Su\" 100000 300000"
expect_line A "^201 Item added 2$"
expect_line A "^NOTIFY ITEM_ADDED 2 \"Lo hoa\"$"
expect_line B "^NOTIFY ITEM_ADDED 2 \"Lo hoa\"$"

client_send A "ADD_ITEM \"Dong ho\" \"Co\" 200000 500000"
expect_line A "^201 Item added 3$"
expect_line A "^NOTIFY ITEM_ADDED 3 \"Dong ho\"$"
expect_line B "^NOTIFY ITEM_ADDED 3 \"Dong ho\"$"

client_send B "LIST_ITEMS"
if expect_line B "^200 ITEMS 3; 1 ON_AUCTION \"Binh gom\" \"Gom co\" 500000 2000000 500000 -; 2 PENDING \"Lo hoa\" \"Su\" 100000 300000 100000 -; 3 PENDING \"Dong ho\" \"Co\" 200000 500000 200000 -$"; then
    pass_at "AT-17"
else
    fail_at "AT-17" "LIST_ITEMS order or format mismatch"
fi

# AT-18: REMOVE_ITEM chỉ khi PENDING
r=0
client_send A "REMOVE_ITEM 2"
expect_line A "^200 Item removed 2$" || r=1
expect_line A "^NOTIFY ITEM_REMOVED 2$" || r=1
expect_line B "^NOTIFY ITEM_REMOVED 2$" || r=1

client_send A "REMOVE_ITEM 1"
expect_line A "^409 Item is not pending$" || r=1
client_send A "REMOVE_ITEM 2"
expect_line A "^409 Item is not pending$" || r=1
if (( r == 0 )); then
    pass_at "AT-18"
else
    fail_at "AT-18" "REMOVE_ITEM behavior mismatch"
fi

finish_suite
