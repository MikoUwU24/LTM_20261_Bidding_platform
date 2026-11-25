#!/usr/bin/env bash
set -u
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

start_server || exit 1
client_open U
client_open A
client_open B
client_open C

# AT-10: Kiểm tra quyền khi chưa đăng nhập (401) và khi ở ngoài phòng (403)
r=0
client_send U "CREATE_ROOM \"x\""
expect_line U "^401 Not logged in$" || r=1
client_send U "BID 1 100000"
expect_line U "^401 Not logged in$" || r=1

login_as A alice pass1
client_send A "LIST_ITEMS"
expect_line A "^403 Not in a room$" || r=1
client_send A "ADD_ITEM \"a\" \"b\" 100 200"
expect_line A "^403 Not in a room$" || r=1
client_send A "LEAVE_ROOM"
expect_line A "^403 Not in a room$" || r=1
if (( r == 0 )); then
    pass_at "AT-10"
else
    fail_at "AT-10" "unauthenticated or out-of-room guard failed"
fi

# AT-12: CREATE_ROOM, chặn tạo phòng khi đang ở phòng (409), tên phòng rỗng (400)
r=0
client_send A "CREATE_ROOM \"Do co\""
expect_line A "^201 Room created 1$" || r=1
client_send A "CREATE_ROOM \"Khac\""
expect_line A "^409 Already in a room$" || r=1
client_send A "LEAVE_ROOM"
expect_line A "^200 Left room 1$" || r=1
client_send A "CREATE_ROOM \"\""
expect_line A "^400 Invalid room name$" || r=1
client_send A "JOIN_ROOM 1"
expect_line A "^200 Joined room 1 \"Do co\"$" || r=1
if (( r == 0 )); then
    pass_at "AT-12"
else
    fail_at "AT-12" "create room validation failed"
fi

# AT-11: Vai trò owner vs member & phạm vi vật phẩm giữa 2 phòng
login_as B bob pass1
login_as C carol pass1

client_send A "ADD_ITEM \"Binh\" \"Mo ta\" 500000 1000000"
expect_line A "^201 Item added 1$"
expect_line A "^NOTIFY ITEM_ADDED 1 \"Binh\"$"
expect_line A "^NOTIFY AUCTION_START 1 "

client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Do co\"$"
expect_line B "^NOTIFY AUCTION_START 1 "
expect_line A "^NOTIFY USER_JOINED bob$"

client_send C "CREATE_ROOM \"Tranh\""
expect_line C "^201 Room created 2$"
client_send C "ADD_ITEM \"Tranh 1\" \"Son dau\" 200000 500000"
expect_line C "^201 Item added 2$"
expect_line C "^NOTIFY ITEM_ADDED 2 \"Tranh 1\"$"
expect_line C "^NOTIFY AUCTION_START 2 "

r=0
client_send A "BID 1 600000"
expect_line A "^403 Room owner cannot bid$" || r=1
client_send B "ADD_ITEM \"x\" \"y\" 100 200"
expect_line B "^403 Only room owner can do this$" || r=1
client_send B "REMOVE_ITEM 1"
expect_line B "^403 Only room owner can do this$" || r=1
client_send B "BID 2 999999"
expect_line B "^404 Item not found$" || r=1
if (( r == 0 )); then
    pass_at "AT-11"
else
    fail_at "AT-11" "owner/member ACL or cross-room item scope failed"
fi

# AT-13: LIST_ROOMS
client_send A "ADD_ITEM \"P1\" \"\" 100 200"
expect_line A "^201 Item added 3$"
expect_line A "^NOTIFY ITEM_ADDED 3 \"P1\"$"
expect_line B "^NOTIFY ITEM_ADDED 3 \"P1\"$"

client_send A "ADD_ITEM \"P2\" \"\" 100 200"
expect_line A "^201 Item added 4$"
expect_line A "^NOTIFY ITEM_ADDED 4 \"P2\"$"
expect_line B "^NOTIFY ITEM_ADDED 4 \"P2\"$"

client_send C "LEAVE_ROOM"
expect_line C "^200 Left room 2$"

client_send C "LIST_ROOMS"
if expect_line C "^200 ROOMS 2; 1 alice \"Do co\" 2 1 2; 2 carol \"Tranh\" 0 2 0$"; then
    pass_at "AT-13"
else
    fail_at "AT-13" "LIST_ROOMS output mismatch"
fi

# AT-14: JOIN_ROOM / LEAVE_ROOM
r=0
client_send B "JOIN_ROOM 2"
expect_line B "^409 Already in a room$" || r=1
client_send B "LEAVE_ROOM"
expect_line B "^200 Left room 1$" || r=1
expect_line A "^NOTIFY USER_LEFT bob$" || r=1
client_send B "JOIN_ROOM 99"
expect_line B "^404 Room not found$" || r=1
client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Do co\"$" || r=1
expect_line B "^NOTIFY AUCTION_START 1 " || r=1
expect_line A "^NOTIFY USER_JOINED bob$" || r=1
if (( r == 0 )); then
    pass_at "AT-14"
else
    fail_at "AT-14" "join/leave room lifecycle failed"
fi

finish_suite
