#!/usr/bin/env bash
set -u
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

start_server || exit 1
client_open A
client_open B

login_as A alice secret1
login_as B bob secret1

client_send A "CREATE_ROOM \"Gom su\""
expect_line A "^201 Room created 1$"
client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Gom su\"$"
expect_line A "^NOTIFY USER_JOINED bob$"

client_send A "ADD_ITEM \"Binh\" \"Mo ta\" 100000 200000"
expect_line A "^201 Item added 1$"
expect_line A "^NOTIFY ITEM_ADDED 1 "
expect_line A "^NOTIFY AUCTION_START 1 "
expect_line B "^NOTIFY ITEM_ADDED 1 "
expect_line B "^NOTIFY AUCTION_START 1 "

client_send B "BID 1 105000"
expect_line B "^422 Bid must be at least 110000$"

client_send B "BID 1 120000"
expect_line B "^200 Bid accepted$"
expect_line B "^NOTIFY NEW_BID 1 bob 120000$"
expect_line A "^NOTIFY NEW_BID 1 bob 120000$"

expect_line B "^NOTIFY COUNTDOWN 1 1 120000$" 5
expect_line B "^NOTIFY COUNTDOWN 1 2 120000$" 3
expect_line B "^NOTIFY COUNTDOWN 1 3 120000$" 3
expect_line B "^NOTIFY SOLD 1 bob 120000$" 3
expect_line A "^NOTIFY COUNTDOWN 1 1 120000$" 1
expect_line A "^NOTIFY COUNTDOWN 1 2 120000$" 1
expect_line A "^NOTIFY COUNTDOWN 1 3 120000$" 1
expect_line A "^NOTIFY SOLD 1 bob 120000$" 1

client_send A "ADD_ITEM \"Dia\" \"\" 100000 200000"
expect_line A "^201 Item added 2$"
expect_line B "^NOTIFY ITEM_ADDED 2 "
expect_line B "^NOTIFY AUCTION_START 2 "
expect_line B "^NOTIFY COUNTDOWN 2 1 100000$" 5
expect_line B "^NOTIFY COUNTDOWN 2 2 100000$" 3
expect_line B "^NOTIFY COUNTDOWN 2 3 100000$" 3
expect_line B "^NOTIFY UNSOLD 2$" 3

client_close B
sleep 0.1

LOG="$TEST_TMP/logs/server.log"
r=0
grep -q "CONNECT" "$LOG" || r=1
grep -q "DISCONNECT" "$LOG" || r=1
grep -q "REJECT(422)" "$LOG" || r=1
grep -q "COUNTDOWN item=1 stage=1 price=120000" "$LOG" || r=1
grep -q "SOLD item=1 to=bob price=120000" "$LOG" || r=1
grep -q "UNSOLD item=2" "$LOG" || r=1
if grep -q "secret1" "$LOG"; then
    r=1
fi
if grep -vE '^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] \[(INFO |WARN |ERROR)\] [^ ]+ [A-Z0-9_]+' "$LOG" >/dev/null; then
    r=1
fi

if (( r == 0 )); then
    pass_at "AT-28"
else
    fail_at "AT-28" "server.log format or required events check failed"
    cat "$LOG" >&2
fi

finish_suite
