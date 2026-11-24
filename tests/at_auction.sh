#!/usr/bin/env bash
set -u
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

start_server || exit 1
client_open A
client_open B
client_open C

login_as A alice pass1
login_as B bob pass1
login_as C carol pass1

client_send A "CREATE_ROOM \"Do co\""
expect_line A "^201 Room created 1$"
client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Do co\"$"
expect_line A "^NOTIFY USER_JOINED bob$"
client_send C "JOIN_ROOM 1"
expect_line C "^200 Joined room 1 \"Do co\"$"
expect_line A "^NOTIFY USER_JOINED carol$"
expect_line B "^NOTIFY USER_JOINED carol$"

client_send A "ADD_ITEM \"Binh gom\" \"Gom\" 500000 2000000"
expect_line A "^201 Item added 1$"
expect_line A "^NOTIFY ITEM_ADDED 1 "
expect_line A "^NOTIFY AUCTION_START 1 "
expect_line B "^NOTIFY ITEM_ADDED 1 "
expect_line B "^NOTIFY AUCTION_START 1 "
expect_line C "^NOTIFY ITEM_ADDED 1 "
expect_line C "^NOTIFY AUCTION_START 1 "

client_send A "ADD_ITEM \"Lo hoa\" \"Su\" 100000 300000"
expect_line A "^201 Item added 2$"
expect_line A "^NOTIFY ITEM_ADDED 2 "
expect_line B "^NOTIFY ITEM_ADDED 2 "
expect_line C "^NOTIFY ITEM_ADDED 2 "

# AT-19: Bước giá tối thiểu 10.000 và broadcast NEW_BID
r=0
client_send B "BID 1 505000"
expect_line B "^422 Bid must be at least 510000$" || r=1
client_send B "BID 1 510000"
expect_line B "^200 Bid accepted$" || r=1
expect_line B "^NOTIFY NEW_BID 1 bob 510000$" || r=1
expect_line A "^NOTIFY NEW_BID 1 bob 510000$" || r=1
expect_line C "^NOTIFY NEW_BID 1 bob 510000$" || r=1
client_send C "BID 1 515000"
expect_line C "^422 Bid must be at least 520000$" || r=1
if (( r == 0 )); then
    pass_at "AT-19"
else
    fail_at "AT-19" "bid step validation or broadcast failed"
fi

# AT-20: BID trên vật phẩm PENDING (409) & giá sai định dạng (400)
r=0
client_send B "BID 2 200000"
expect_line B "^409 Item is not on auction$" || r=1
client_send B "BID 1 abc"
expect_line B "^400 Invalid number$" || r=1
if (( r == 0 )); then
    pass_at "AT-20"
else
    fail_at "AT-20" "bid on pending item or invalid number check failed"
fi

# AT-22: Reset bộ đếm khi có BID trong lúc COUNTDOWN
# Chờ COUNTDOWN 1 và 2 của giá 510000, sau đó carol BID 700000
r=0
expect_line B "^NOTIFY COUNTDOWN 1 1 510000$" 5 || r=1
expect_line B "^NOTIFY COUNTDOWN 1 2 510000$" 3 || r=1
expect_line A "^NOTIFY COUNTDOWN 1 1 510000$" 1 || r=1
expect_line A "^NOTIFY COUNTDOWN 1 2 510000$" 1 || r=1
expect_line C "^NOTIFY COUNTDOWN 1 1 510000$" 1 || r=1
expect_line C "^NOTIFY COUNTDOWN 1 2 510000$" 1 || r=1

client_send C "BID 1 700000"
expect_line C "^200 Bid accepted$" || r=1
expect_line C "^NOTIFY NEW_BID 1 carol 700000$" || r=1
expect_line A "^NOTIFY NEW_BID 1 carol 700000$" || r=1
expect_line B "^NOTIFY NEW_BID 1 carol 700000$" || r=1

# Trong 2 giây kế tiếp không được có COUNTDOWN
expect_no_line B 2 || r=1
expect_line B "^NOTIFY COUNTDOWN 1 1 700000$" 3 || r=1
expect_line A "^NOTIFY COUNTDOWN 1 1 700000$" 1 || r=1
expect_line C "^NOTIFY COUNTDOWN 1 1 700000$" 1 || r=1
if (( r == 0 )); then
    pass_at "AT-22"
else
    fail_at "AT-22" "countdown timer reset on valid bid failed"
fi

# AT-21: Đủ 3 lần COUNTDOWN -> SOLD và tự động chuyển vật phẩm 2 lên sàn
r=0
expect_line B "^NOTIFY COUNTDOWN 1 2 700000$" 3 || r=1
expect_line B "^NOTIFY COUNTDOWN 1 3 700000$" 3 || r=1
expect_line B "^NOTIFY SOLD 1 carol 700000$" 3 || r=1
expect_line B "^NOTIFY AUCTION_START 2 \"Lo hoa\" \"Su\" 100000 300000$" 2 || r=1

expect_line A "^NOTIFY COUNTDOWN 1 2 700000$" 1 || r=1
expect_line A "^NOTIFY COUNTDOWN 1 3 700000$" 1 || r=1
expect_line A "^NOTIFY SOLD 1 carol 700000$" 1 || r=1
expect_line A "^NOTIFY AUCTION_START 2 " 1 || r=1

expect_line C "^NOTIFY COUNTDOWN 1 2 700000$" 1 || r=1
expect_line C "^NOTIFY COUNTDOWN 1 3 700000$" 1 || r=1
expect_line C "^NOTIFY SOLD 1 carol 700000$" 1 || r=1
expect_line C "^NOTIFY AUCTION_START 2 " 1 || r=1
if (( r == 0 )); then
    pass_at "AT-21"
else
    fail_at "AT-21" "3x countdown to SOLD transition failed"
fi

# AT-23: Không ai đặt giá item 2 -> COUNTDOWN x3 -> UNSOLD -> tự đưa item 3 lên sàn
client_send A "ADD_ITEM \"Tranh\" \"Co\" 100000 300000"
expect_line A "^201 Item added 3$"
expect_line A "^NOTIFY ITEM_ADDED 3 "
expect_line B "^NOTIFY ITEM_ADDED 3 "
expect_line C "^NOTIFY ITEM_ADDED 3 "

r=0
expect_line B "^NOTIFY COUNTDOWN 2 1 100000$" 5 || r=1
expect_line B "^NOTIFY COUNTDOWN 2 2 100000$" 3 || r=1
expect_line B "^NOTIFY COUNTDOWN 2 3 100000$" 3 || r=1
expect_line B "^NOTIFY UNSOLD 2$" 3 || r=1
expect_line B "^NOTIFY AUCTION_START 3 \"Tranh\" \"Co\" 100000 300000$" 2 || r=1
if (( r == 0 )); then
    pass_at "AT-23"
else
    fail_at "AT-23" "UNSOLD transition to next item failed"
fi

# Đọc hết buffer của A và C cho item 2
for _ in 1 2 3 4 5; do
    expect_line A "^NOTIFY " 1 >/dev/null || true
    expect_line C "^NOTIFY " 1 >/dev/null || true
done

# AT-24: Người đặt giá mất kết nối, giá vẫn giữ hiệu lực và thắng
client_send B "BID 3 150000"
expect_line B "^200 Bid accepted$"
expect_line B "^NOTIFY NEW_BID 3 bob 150000$"
expect_line A "^NOTIFY NEW_BID 3 bob 150000$"
expect_line C "^NOTIFY NEW_BID 3 bob 150000$"

client_close B
expect_line A "^NOTIFY USER_LEFT bob$" 2
expect_line C "^NOTIFY USER_LEFT bob$" 2

r=0
expect_line C "^NOTIFY COUNTDOWN 3 1 150000$" 5 || r=1
expect_line C "^NOTIFY COUNTDOWN 3 2 150000$" 3 || r=1
expect_line C "^NOTIFY COUNTDOWN 3 3 150000$" 3 || r=1
expect_line C "^NOTIFY SOLD 3 bob 150000$" 3 || r=1
if (( r == 0 )); then
    pass_at "AT-24"
else
    fail_at "AT-24" "disconnected bidder did not win auction"
fi

for _ in 1 2 3 4; do expect_line A "^NOTIFY " 1 >/dev/null || true; done

# AT-25: BUY_NOW kết thúc ngay và đưa vật phẩm kế tiếp lên
client_open B
login_as B bob pass1
client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Do co\"$"
expect_line A "^NOTIFY USER_JOINED bob$"
expect_line C "^NOTIFY USER_JOINED bob$"

client_send A "ADD_ITEM \"V4\" \"\" 100000 300000"
expect_line A "^201 Item added 4$"
expect_line A "^NOTIFY ITEM_ADDED 4 "
expect_line A "^NOTIFY AUCTION_START 4 "
expect_line B "^NOTIFY ITEM_ADDED 4 "
expect_line B "^NOTIFY AUCTION_START 4 "
expect_line C "^NOTIFY ITEM_ADDED 4 "
expect_line C "^NOTIFY AUCTION_START 4 "

client_send A "ADD_ITEM \"V5\" \"\" 100000 300000"
expect_line A "^201 Item added 5$"
expect_line A "^NOTIFY ITEM_ADDED 5 "
expect_line B "^NOTIFY ITEM_ADDED 5 "
expect_line C "^NOTIFY ITEM_ADDED 5 "

r=0
client_send B "BUY_NOW 4"
expect_line B "^200 Bought item 4 price 300000$" || r=1
expect_line B "^NOTIFY SOLD 4 bob 300000$" || r=1
expect_line B "^NOTIFY AUCTION_START 5 " || r=1
expect_line C "^NOTIFY SOLD 4 bob 300000$" || r=1
expect_line C "^NOTIFY AUCTION_START 5 " || r=1
expect_line A "^NOTIFY SOLD 4 bob 300000$" || r=1
expect_line A "^NOTIFY AUCTION_START 5 " || r=1
if (( r == 0 )); then
    pass_at "AT-25"
else
    fail_at "AT-25" "BUY_NOW immediate sale failed"
fi

# AT-26: BID >= buy_now_price xử lý như BUY_NOW
r=0
client_send C "BID 5 500000"
expect_line C "^200 Bought item 5 price 300000$" || r=1
expect_line C "^NOTIFY SOLD 5 carol 300000$" || r=1
expect_line B "^NOTIFY SOLD 5 carol 300000$" || r=1
expect_line A "^NOTIFY SOLD 5 carol 300000$" || r=1
if (( r == 0 )); then
    pass_at "AT-26"
else
    fail_at "AT-26" "BID >= buy_now_price did not trigger buy now"
fi

# AT-27: Hai BUY_NOW gần như cùng lúc
client_send A "ADD_ITEM \"V6\" \"\" 100000 300000"
expect_line A "^201 Item added 6$"
expect_line A "^NOTIFY ITEM_ADDED 6 "
expect_line A "^NOTIFY AUCTION_START 6 "
expect_line B "^NOTIFY ITEM_ADDED 6 "
expect_line B "^NOTIFY AUCTION_START 6 "
expect_line C "^NOTIFY ITEM_ADDED 6 "
expect_line C "^NOTIFY AUCTION_START 6 "

client_send B "BUY_NOW 6"
client_send C "BUY_NOW 6"
sleep 0.3

b_ok=$(grep -c "^200 Bought item 6 price 300000" "$TEST_TMP/B.out" || true)
c_ok=$(grep -c "^200 Bought item 6 price 300000" "$TEST_TMP/C.out" || true)
b_409=$(grep -c "^409 Item is not on auction" "$TEST_TMP/B.out" || true)
c_409=$(grep -c "^409 Item is not on auction" "$TEST_TMP/C.out" || true)
sold_6=$(grep -c "NOTIFY SOLD 6 " "$TEST_TMP/A.out" || true)

if (( b_ok + c_ok == 1 && b_409 + c_409 == 1 && sold_6 == 1 )); then
    pass_at "AT-27"
else
    fail_at "AT-27" "concurrent BUY_NOW race check failed (b_ok=$b_ok c_ok=$c_ok b_409=$b_409 c_409=$c_409 sold=$sold_6)"
fi

# Đồng bộ chỉ số dòng cho A, B, C
CLIENT_READ_LINE[A]=$(wc -l <"$TEST_TMP/A.out")
CLIENT_READ_LINE[B]=$(wc -l <"$TEST_TMP/B.out")
CLIENT_READ_LINE[C]=$(wc -l <"$TEST_TMP/C.out")

# AT-29: Khởi động lại server giữ trạng thái ON_AUCTION + current_price + winner
client_send A "ADD_ITEM \"V7\" \"\" 100000 300000"
expect_line A "^201 Item added 7$"
expect_line A "^NOTIFY ITEM_ADDED 7 "
expect_line A "^NOTIFY AUCTION_START 7 "
expect_line B "^NOTIFY ITEM_ADDED 7 "
expect_line B "^NOTIFY AUCTION_START 7 "

client_send B "BID 7 150000"
expect_line B "^200 Bid accepted$"
expect_line B "^NOTIFY NEW_BID 7 bob 150000$"

client_close A
client_close B
client_close C
restart_server || exit 1

client_open C
client_send C "LOGIN carol pass1"
expect_line C "^200 Login successful$"
client_send C "JOIN_ROOM 1"
r=0
expect_line C "^200 Joined room 1 \"Do co\"$" || r=1
expect_line C "^NOTIFY AUCTION_START 7 \"V7\" \"\" 100000 300000$" || r=1
expect_line C "^NOTIFY NEW_BID 7 bob 150000$" || r=1
expect_line C "^NOTIFY COUNTDOWN 7 1 150000$" 5 || r=1
if (( r == 0 )); then
    pass_at "AT-29"
else
    fail_at "AT-29" "auction state recovery after restart failed"
fi

finish_suite
