#!/usr/bin/env bash
set -u
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

start_server || exit 1
client_open A
client_open B

# AT-05: REGISTER và lưu hash (không lưu mật khẩu rõ)
client_send A "REGISTER bob secret1"
if expect_line A "^201 Registered bob$" &&
   grep -q "^bob " "$TEST_TMP/data/accounts.txt" &&
   ! grep -q "secret1" "$TEST_TMP/data/accounts.txt"; then
    pass_at "AT-05"
else
    fail_at "AT-05" "register or password hash storage failed"
fi

# AT-06: Trùng username (409) và sai định dạng username (400)
client_send A "REGISTER bob x2345"
r1=$?; expect_line A "^409 Username already exists$" || r1=1
client_send A "REGISTER ab pass1"
expect_line A "^400 Invalid username$" || r1=1
client_send A "REGISTER bo-b pass1"
expect_line A "^400 Invalid username$" || r1=1
if (( r1 == 0 )); then
    pass_at "AT-06"
else
    fail_at "AT-06" "duplicate/invalid username validation failed"
fi

# AT-07: Khởi động lại server, LOGIN đúng & sai mật khẩu
client_close A
client_close B
restart_server || exit 1
client_open A
client_open B

client_send A "LOGIN bob secret1"
r2=0
expect_line A "^200 Login successful$" || r2=1
client_send B "LOGIN bob wrongpass"
expect_line B "^401 Invalid username or password$" || r2=1
if (( r2 == 0 )); then
    pass_at "AT-07"
else
    fail_at "AT-07" "login after restart or wrong password check failed"
fi

# AT-08: Đăng nhập trùng tài khoản ở kết nối khác & đăng nhập lần 2 trên cùng phiên
client_send B "LOGIN bob secret1"
r3=0
expect_line B "^409 Account is logged in elsewhere$" || r3=1
client_send A "LOGIN bob secret1"
expect_line A "^409 Already logged in$" || r3=1
if (( r3 == 0 )); then
    pass_at "AT-08"
else
    fail_at "AT-08" "concurrent login conflict check failed"
fi

# AT-09: LOGOUT và mất kết nối đột ngột
login_as B carol secret2
client_send A "CREATE_ROOM \"Phong 1\""
expect_line A "^201 Room created 1$"
client_send B "JOIN_ROOM 1"
expect_line B "^200 Joined room 1 \"Phong 1\"$"
expect_line A "^NOTIFY USER_JOINED carol$"

client_send A "LOGOUT"
r4=0
expect_line A "^200 Logged out$" || r4=1
expect_line B "^NOTIFY USER_LEFT bob$" || r4=1

# B đóng kết nối đột ngột, C đăng nhập lại carol thành công
client_close B
sleep 0.1
client_open C
client_send C "LOGIN carol secret2"
expect_line C "^200 Login successful$" || r4=1
if (( r4 == 0 )); then
    pass_at "AT-09"
else
    fail_at "AT-09" "logout or abrupt disconnect cleanup failed"
fi

finish_suite
