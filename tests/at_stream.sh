#!/usr/bin/env bash
set -u
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

start_server || exit 1

# AT-01: Nhận thiếu (fragmented command)
client_open A
client_send_raw A "LIST_"
sleep 0.2
client_send_raw A "ROOMS\r\n"
if expect_line A "^401 Not logged in$" && expect_no_line A 1; then
    pass_at "AT-01"
else
    fail_at "AT-01" "fragmented line not reassembled properly"
fi

# AT-02: Nhận dính gói (3 commands in one send)
client_send_raw A "REGISTER usr1 pass1\r\nLOGIN usr1 pass1\r\nLIST_ROOMS\r\n"
if expect_line A "^201 Registered usr1$" &&
   expect_line A "^200 Login successful$" &&
   expect_line A "^200 ROOMS 0$"; then
    pass_at "AT-02"
else
    fail_at "AT-02" "sticky commands did not produce 3 ordered replies"
fi

# AT-03: Dòng vượt MAX_LINE (>4096 bytes)
client_open B
LONG_STR="$(head -c 5000 < /dev/zero | tr '\0' 'A')"
client_send_raw B "$LONG_STR"
if expect_line B "^400 Line too long$"; then
    # Client A vẫn hoạt động bình thường
    client_send A "LIST_ROOMS"
    if expect_line A "^200 ROOMS 0$" && grep -q "LINE_TOO_LONG REJECT(400)" "$TEST_TMP/logs/server.log"; then
        pass_at "AT-03"
    else
        fail_at "AT-03" "other client affected or log missing"
    fi
else
    fail_at "AT-03" "did not receive 400 Line too long"
fi

stop_server

# AT-04: Quá tải MAX_CLIENTS=2 -> 503 Server busy
SERVER_BIN="$ROOT_DIR/bin/server_small" start_server || exit 1
client_open C1
client_open C2
client_open C3

if expect_line C3 "^503 Server busy$"; then
    client_send C1 "LIST_ROOMS"
    client_send C2 "LIST_ROOMS"
    if expect_line C1 "^401 Not logged in$" && expect_line C2 "^401 Not logged in$"; then
        pass_at "AT-04"
    else
        fail_at "AT-04" "C1/C2 not served concurrently"
    fi
else
    fail_at "AT-04" "C3 did not receive 503 Server busy"
fi

finish_suite
