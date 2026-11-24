# 02 — Acceptance Criteria & Test Plan

> Tiêu chí nghiệm thu **Given / When / Then** cho agent viết test tự động (`tests/at_*.sh`) và cho người demo.
> Build test: `make test` (timer rút ngắn `AUCTION_IDLE_SEC=3`, `COUNTDOWN_INTERVAL_SEC=1`). So khớp phản hồi theo **mã + tiền tố** của spec 02.

---

## 1. Hạ tầng test (`tests/lib.sh`)

- Mỗi file `at_*.sh` chạy server riêng trong thư mục tạm (`mktemp -d`, tạo `data/ logs/`), cổng `PORT=$((20000 + RANDOM % 10000))`.
- Mỗi client: `mkfifo $T/<name>.in`; `nc 127.0.0.1 $PORT < $T/<name>.in > $T/<name>.out &`; giữ FIFO mở bằng `exec {fd}>$T/<name>.in`.

| Hàm | Ý nghĩa |
|---|---|
| `start_server` / `stop_server` | Chạy `bin/server $PORT` nền, chờ cổng mở (thử `nc -z` tối đa 2s) / `kill` + xoá thư mục tạm |
| `client_open <name>` / `client_close <name>` | Mở/đóng kết nối `nc` |
| `client_send <name> "<line>"` | `printf '%s\r\n'` vào FIFO |
| `client_send_raw <name> "<bytes>"` | `printf '%b'` không thêm kết thúc dòng (test truyền dòng) |
| `expect_line <name> "<regex>" [timeout=3]` | Chờ tới khi **dòng chưa đọc kế tiếp** khớp regex (`grep -E`); sai thứ tự = FAIL |
| `expect_no_line <name> [giây=1]` | Không có dòng mới trong khoảng thời gian |
| `login_as <name> <user> <pass>` | Tiện ích: `REGISTER` + `LOGIN` + kỳ vọng `201`, `200` |

Mỗi AT in `PASS AT-xx` hoặc `FAIL AT-xx: <lý do>`.

---

## 2. Ma trận nghiệm thu tự động

| ID | # | REQ | Given / When / Then | Kỳ vọng | Trạng thái |
|---|:-:|---|---|---|:-:|
| **AT-01** | 1 | STREAM-002 | **Given** client A. **When** gửi `LIST_` rồi 200ms sau gửi `ROOMS\r\n` (nhận thiếu). **Then** server ghép thành một lệnh. | `401 Not logged in` đúng **một** dòng | ☑ |
| **AT-02** | 1 | STREAM-002 | **When** gửi trong một lần `REGISTER usr1 pass1\r\nLOGIN usr1 pass1\r\nLIST_ROOMS\r\n` (dính gói). **Then** 3 phản hồi theo đúng thứ tự. | `201` → `200` → `200 ROOMS 0` | ☑ |
| **AT-03** | 1 | STREAM-003 | **When** gửi 5000 ký tự `A` không xuống dòng. **Then** server báo lỗi và đóng kết nối; client khác vẫn hoạt động. | `400 Line too long`, kết nối A đóng, log `LINE_TOO_LONG` | ☑ |
| **AT-04** | 2 | IO-002, IO-005 | **Given** server build `-DMAX_CLIENTS=2`. **When** 3 client kết nối, 2 client đầu gửi lệnh xen kẽ. **Then** client thứ 3 bị từ chối, 2 client đầu được phục vụ đồng thời. | Client 3: `503 Server busy`; client 1–2 nhận phản hồi | ☑ |
| **AT-05** | 3 | ACC-001..003 | **When** `REGISTER bob secret1`. **Then** tạo tài khoản; `data/accounts.txt` có `bob` nhưng **không** chứa `secret1`. | `201 Registered bob` | ☑ |
| **AT-06** | 3 | ACC-001, ACC-002 | **When** `REGISTER bob x2345` lần 2; `REGISTER ab pass1`; `REGISTER bo-b pass1`. | `409`, `400`, `400` | ☑ |
| **AT-07** | 3, 4 | ACC-003, SES-002 | **Given** đã `REGISTER bob secret1`, khởi động lại server. **When** `LOGIN bob secret1`, rồi client khác `LOGIN bob wrong`. | `200`; `401 Invalid username or password` | ☑ |
| **AT-08** | 4 | SES-003 | **Given** A đã `LOGIN bob`. **When** B `LOGIN bob secret1`; A gửi `LOGIN bob secret1`. | B: `409 Account is logged in elsewhere`; A: `409 Already logged in` | ☑ |
| **AT-09** | 4 | SES-004, SES-005 | **Given** A (bob) và B (carol) cùng phòng 1. **When** A `LOGOUT`. **Then** B nhận `USER_LEFT bob`; **When** B đóng kết nối đột ngột, C đăng nhập `carol` thành công. | `200 Logged out`; `NOTIFY USER_LEFT bob`; C: `200` | ☑ |
| **AT-10** | 5 | ACL-001, ACL-002 | **Given** client chưa đăng nhập. **When** `CREATE_ROOM "x"`, `BID 1 1`. **Given** đã đăng nhập, ngoài phòng. **When** `LIST_ITEMS`, `ADD_ITEM "a" "b" 1 2`, `LEAVE_ROOM`. | `401`, `401`; `403`, `403`, `403` | ☑ |
| **AT-11** | 5 | ACL-002, ACL-003 | **Given** alice owner phòng 1 (item 1 đang đấu giá), bob member phòng 1, carol owner phòng 2 (item 2). **When** alice `BID 1 600000`; bob `ADD_ITEM "a" "b" 1 2`; bob `REMOVE_ITEM 1`; bob `BID 2 999999`. | `403 Room owner cannot bid`; `403`; `403`; `404 Item not found` | ☑ |
| **AT-12** | 6 | ROOM-001 | **When** alice `CREATE_ROOM "Do co"`, rồi `CREATE_ROOM "Khac"`; `CREATE_ROOM ""`. **Then** phòng 1 tạo, alice ở trong phòng. | `201 Room created 1`; `409 Already in a room`; (sau `LEAVE_ROOM`) `400 Invalid room name` | ☑ |
| **AT-13** | 7 | ROOM-002 | **Given** phòng 1 (alice, bob trong phòng, 1 ON_AUCTION + 2 PENDING), phòng 2 (rỗng). **When** carol `LIST_ROOMS`. | `200 ROOMS 2; 1 alice "Do co" 2 <id> 2; 2 dave "<name>" 0 - 0` | ☑ |
| **AT-14** | 10 | ROOM-003..005 | **Given** alice ở phòng 1. **When** bob `JOIN_ROOM 1`; bob `JOIN_ROOM 2`; bob `JOIN_ROOM 99` (sau khi rời); bob `LEAVE_ROOM`. | `200 Joined room 1 "Do co"`, alice: `NOTIFY USER_JOINED bob`; `409`; `404`; `200 Left room 1`, alice: `NOTIFY USER_LEFT bob` | ☑ |
| **AT-15** | 8 | ITEM-001..004 | **Given** alice owner phòng 1 rỗng, bob trong phòng. **When** alice `ADD_ITEM "Binh gom" "Gom co" 500000 2000000`. **Then** alice nhận phản hồi trước, sau đó cả hai nhận thông báo. | alice: `201 Item added 1` → `ITEM_ADDED 1 "Binh gom"` → `AUCTION_START 1 ...`; bob: `ITEM_ADDED` → `AUCTION_START` | ☑ |
| **AT-16** | 8 | ITEM-001, ITEM-002 | **When** `ADD_ITEM "a" "b" 500 500`; `ADD_ITEM "a" "b" -1 10`; `ADD_ITEM "a" "b" 1x 10`; `ADD_ITEM "a" b 1`. | `422`; `400`; `400`; `400 Wrong number of arguments` | ☑ |
| **AT-17** | 8 | ITEM-003 | **Given** item 1 ON_AUCTION. **When** thêm item 2, 3. **Then** `LIST_ITEMS` theo thứ tự. | `200 ITEMS 3; 1 ON_AUCTION ...; 2 PENDING ...; 3 PENDING ...` | ☑ |
| **AT-18** | 9 | ITEM-005 | **Given** item 1 ON_AUCTION, item 2 PENDING. **When** alice `REMOVE_ITEM 2`; `REMOVE_ITEM 1`; `REMOVE_ITEM 2` lần nữa. | `200 Item removed 2` + `NOTIFY ITEM_REMOVED 2` (bob nhận); `409 Item is not pending`; `409` | ☑ |
| **AT-19** | 11 | AUC-002, AUC-004 | **Given** item 1 (start 500000, buy 2000000) ON_AUCTION, bob và carol trong phòng. **When** bob `BID 1 505000`; bob `BID 1 510000`; carol `BID 1 515000`. | `422 Bid must be at least 510000`; `200 Bid accepted` + mọi người `NEW_BID 1 bob 510000`; `422 Bid must be at least 520000` | ☑ |
| **AT-20** | 11 | AUC-001 | **When** bob `BID 2 999999` (item 2 PENDING cùng phòng); `BID 1 abc`. | `409 Item is not on auction`; `400 Invalid number` | ☑ |
| **AT-21** | 11 | AUC-005 | **Given** timer rút ngắn (3s/1s). **When** bob đặt giá 600000 rồi không ai đặt. **Then** đúng 3 lần đếm ngược rồi bán. | `COUNTDOWN 1 1 600000` → `COUNTDOWN 1 2 600000` → `COUNTDOWN 1 3 600000` → `SOLD 1 bob 600000`, khoảng cách ≈ 3s/1s/1s/1s | ☑ |
| **AT-22** | 11 | AUC-004, AUC-005 | **When** sau `COUNTDOWN 1 2 ...` carol `BID 1 700000`. **Then** bộ đếm reset: không có `COUNTDOWN` trong ~2.5s tiếp theo, rồi bắt đầu lại từ lần 1. | `NEW_BID 1 carol 700000` → (im lặng) → `COUNTDOWN 1 1 700000` | ☑ |
| **AT-23** | 11 | AUC-005, ITEM-004 | **Given** item 1 ON_AUCTION không ai đặt, item 2 PENDING. **When** hết thời gian. | `COUNTDOWN ×3` → `UNSOLD 1` → `AUCTION_START 2 ...` | ☑ |
| **AT-24** | 11 | AUC-006 | **Given** bob đặt giá cao nhất rồi ngắt kết nối. **When** hết đếm ngược. | carol nhận `USER_LEFT bob` … `SOLD 1 bob <price>` | ☑ |
| **AT-25** | 12 | BUY-001, BUY-002 | **Given** item 1 ON_AUCTION, item 2 PENDING. **When** bob `BUY_NOW 1`. | bob: `200 Bought item 1 price 2000000` → `SOLD 1 bob 2000000` → `AUCTION_START 2 ...`; carol nhận `SOLD`, `AUCTION_START` | ☑ |
| **AT-26** | 12 | AUC-003 | **When** bob `BID 1 2500000` (≥ giá mua ngay). | `200 Bought item 1 price 2000000`, `SOLD 1 bob 2000000` | ☑ |
| **AT-27** | 12 | BUY-003 | **When** bob và carol gửi `BUY_NOW 1` gần như cùng lúc (không có item 2). | Đúng một người `200`, người kia `409 Item is not on auction`; chỉ một `SOLD` | ☑ |
| **AT-28** | 13 | LOG-001..003 | **Given** chạy xong AT-05..AT-27 trong cùng phiên server. **Then** `logs/server.log` đúng định dạng, có `CONNECT`, `DISCONNECT`, `REJECT(422)`, `COUNTDOWN`, `SOLD`, `UNSOLD`; **không** chứa `secret1`. | `grep` khớp regex spec 04 §3.1 cho mọi dòng | ☑ |
| **AT-29** | 3, 8 | ADR-10 | **Given** item 1 ON_AUCTION có giá 600000 của bob, item 2 PENDING. **When** khởi động lại server, carol `JOIN_ROOM 1`. | `AUCTION_START 1 ...` → `NEW_BID 1 bob 600000`; sau ~3s `COUNTDOWN 1 1 600000` | ☑ |

Ánh xạ file: AT-01..04 → `at_stream.sh`; AT-05..09 → `at_session.sh`; AT-10..14 → `at_room.sh`; AT-15..18 → `at_item.sh`; AT-19..27, 29 → `at_auction.sh`; AT-28 → `at_e2e.sh`.

---

## 3. Kịch bản demo thủ công (3 terminal, build mặc định 180s/10s)

1. **T1** `./bin/server 5500`. **T2** `./bin/client 127.0.0.1 5500` → `REGISTER alice 123456`, `LOGIN alice 123456`, `CREATE_ROOM "Do co"`.
2. **T2** `ADD_ITEM "Binh gom" "Gom Bat Trang" 500000 2000000`, `ADD_ITEM "Tranh" "Son dau" 100000 300000` → thấy `AUCTION_START` của Bình gốm.
3. **T3** client bob: đăng ký, đăng nhập, `LIST_ROOMS` (thấy phòng 1, 1 người, item 1, 1 chờ), `JOIN_ROOM 1` → nhận ngay `AUCTION_START`.
4. **T4** client carol: vào phòng 1. bob `BID 1 505000` (bị từ chối 422) → `BID 1 600000`; carol `BID 1 610000` → cả phòng thấy giá mới.
5. **T2** alice `BID 1 700000` → `403`; `REMOVE_ITEM 1` → `409`; `REMOVE_ITEM 2` thành công rồi thêm lại.
6. Chờ 3 phút → 3 lần `COUNTDOWN` cách 10 giây → `SOLD 1 carol 610000` → vật phẩm kế tiếp lên sàn.
7. carol `BUY_NOW <id>` → `SOLD` ngay.
8. Tắt T3 đột ngột (Ctrl+C) → phòng nhận `USER_LEFT bob`. Mở `logs/server.log` trình bày từng loại sự kiện.
9. Kiểm truyền dòng bằng tay: `printf 'LOG' | nc -q1 ...` và `printf 'LIST_ROOMS\r\nLIST_ROOMS\r\n' | nc ...`.
