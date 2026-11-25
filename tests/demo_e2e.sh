#!/usr/bin/env bash
set -u
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

make clean >/dev/null && make CFLAGS_EXTRA="-DAUCTION_IDLE_SEC=3 -DCOUNTDOWN_INTERVAL_SEC=1" >/dev/null
DEMO="$(mktemp -d)"
mkdir -p "$DEMO/data" "$DEMO/logs"
PORT=5599
(cd "$DEMO" && exec "$ROOT_DIR/bin/server" "$PORT") &
SPID=$!
sleep 0.3

mkfifo "$DEMO/alice.in" "$DEMO/bob.in" "$DEMO/carol.in"
"$ROOT_DIR/bin/client" 127.0.0.1 "$PORT" <"$DEMO/alice.in" >"$DEMO/alice.out" 2>&1 &
"$ROOT_DIR/bin/client" 127.0.0.1 "$PORT" <"$DEMO/bob.in" >"$DEMO/bob.out" 2>&1 &
"$ROOT_DIR/bin/client" 127.0.0.1 "$PORT" <"$DEMO/carol.in" >"$DEMO/carol.out" 2>&1 &
exec 3>"$DEMO/alice.in" 4>"$DEMO/bob.in" 5>"$DEMO/carol.in"

send_cmd() {
    local who="$1" fd="$2" cmd="$3"
    echo "[$who] >>> $cmd"
    printf "%s\n" "$cmd" >&"$fd"
    sleep 0.2
}

echo "================ 1. ĐĂNG KÝ & ĐĂNG NHẬP ================"
send_cmd "ALICE" 3 "REGISTER alice pass123"
send_cmd "ALICE" 3 "LOGIN alice pass123"
send_cmd "BOB"   4 "REGISTER bob pass123"
send_cmd "BOB"   4 "LOGIN bob pass123"
send_cmd "CAROL" 5 "REGISTER carol pass123"
send_cmd "CAROL" 5 "LOGIN carol pass123"

echo "================ 2. TẠO PHÒNG & ĐĂNG BÁN 3 VẬT PHẨM ================"
send_cmd "ALICE" 3 "CREATE_ROOM \"Phien Dau Gia Do Co\""
send_cmd "ALICE" 3 "ADD_ITEM \"Binh gom Bat Trang\" \"The ky 19\" 500000 2000000"
send_cmd "ALICE" 3 "ADD_ITEM \"Tranh son dau\" \"Phong canh\" 300000 800000"
send_cmd "ALICE" 3 "ADD_ITEM \"Dong ho qua lac\" \"Phap co\" 400000 1000000"
send_cmd "ALICE" 3 "REMOVE_ITEM 2"

echo "================ 3. LIỆT KÊ PHÒNG & VÀO PHÒNG ================"
send_cmd "BOB"   4 "LIST_ROOMS"
send_cmd "BOB"   4 "JOIN_ROOM 1"
send_cmd "CAROL" 5 "JOIN_ROOM 1"
send_cmd "BOB"   4 "LIST_ITEMS"

echo "================ 4. TỐ GIÁ & ĐẾM NGƯỢC (3s + 3x1s) ================"
send_cmd "BOB"   4 "BID 1 505000"
send_cmd "BOB"   4 "BID 1 550000"
send_cmd "CAROL" 5 "BID 1 600000"
echo "[WAIT] Chờ 7 giây để server gửi 3 lần COUNTDOWN và chốt SOLD item #1 cho carol, tự đẩy item #3 lên sàn..."
sleep 7

echo "================ 5. MUA NGAY (BUY_NOW) VẬT PHẨM KẾ TIẾP ================"
send_cmd "BOB"   4 "BUY_NOW 3"
send_cmd "BOB"   4 "LEAVE_ROOM"
send_cmd "ALICE" 3 "LOGOUT"

send_cmd "ALICE" 3 "quit"
send_cmd "BOB"   4 "quit"
send_cmd "CAROL" 5 "quit"
sleep 0.3
kill "$SPID" 2>/dev/null || true
wait "$SPID" 2>/dev/null || true

echo ""
echo "==================== MÀN HÌNH CLIENT: ALICE (Chủ phòng) ===================="
cat "$DEMO/alice.out"
echo ""
echo "==================== MÀN HÌNH CLIENT: BOB (Người mua 1) ===================="
cat "$DEMO/bob.out"
echo ""
echo "==================== MÀN HÌNH CLIENT: CAROL (Người mua 2) =================="
cat "$DEMO/carol.out"
echo ""
echo "==================== DỮ LIỆU LƯU TRỮ (data/*.txt) =========================="
echo "--- data/accounts.txt ---"
cat "$DEMO/data/accounts.txt"
echo "--- data/rooms.txt ---"
cat "$DEMO/data/rooms.txt"
echo "--- data/items.txt ---"
cat "$DEMO/data/items.txt"
echo ""
echo "==================== FILE LOG (logs/server.log) ============================"
cat "$DEMO/logs/server.log"

rm -rf "$DEMO"
make clean >/dev/null && make >/dev/null
