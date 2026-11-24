# 04 — Storage & Log Specification

> **Phụ thuộc:** ADR-05, ADR-06, ADR-10. Hàm đọc/ghi chung ở `server/storage.c`; định dạng bản ghi do từng module tự viết.

---

## 1. Quy tắc chung

- Thư mục `data/` và `logs/` phải tồn tại (Makefile tạo bằng `mkdir -p`); server kiểm tra bằng `access()` lúc khởi động.
- Một dòng = một bản ghi, trường cách nhau **một dấu cách**, chuỗi tự do bọc `"..."` (nội dung không có `"` — đã chặn ở giao thức). Kết thúc dòng `\n`.
- Đọc: dùng `storage_read_lines` + `parse_tokenize`. Dòng sai định dạng → bỏ qua, log `WARN <file>:<số dòng>`; không làm server dừng.
- Ghi: `storage_write_file` (file `.tmp` → `rename`). Ghi **cả file** sau mỗi thay đổi của module đó.

## 2. Định dạng file

### 2.1 `data/accounts.txt` — module `account`
```
<username> <salt_hex16> <hash_hex16>
```
```
alice 9f3a0c71d2e4b856 1c9e5f0a7b3d2e48
```
Hash: `h = 0xcbf29ce484222325`; lặp `HASH_ROUNDS` lần: với mỗi byte `b` của chuỗi `salt + password`: `h ^= b; h *= 0x100000001b3` (tràn `uint64_t` theo modulo). In `%016llx`. (Vòng sau tiếp tục từ `h` của vòng trước.)

### 2.2 `data/rooms.txt` — module `room`
```
<room_id> <owner> "<name>"
```
```
1 alice "Do co"
```
Trạng thái bộ đếm (`deadline`, `countdown_stage`, `current_item_id`) **không** lưu; dựng lại từ `items.txt` (ADR-10).

### 2.3 `data/items.txt` — module `item`
```
<item_id> <room_id> <STATUS> <start_price> <buy_now_price> <current_price> <winner|-> "<name>" "<desc>"
```
```
12 1 ON_AUCTION 500000 2000000 600000 carol "Binh gom" "Gom Bat Trang the ky 19"
13 1 PENDING 100000 300000 100000 - "Lo hoa" ""
```
- `STATUS` ∈ `PENDING | ON_AUCTION | SOLD | UNSOLD | REMOVED`.
- Vật phẩm `REMOVED`/`SOLD`/`UNSOLD` vẫn giữ trong file (lịch sử); `MAX_ITEMS` tính cả chúng.
- Khi nạp: bỏ bản ghi có `room_id` không tồn tại (log `WARN`); nếu một phòng có > 1 `ON_AUCTION` → giữ id nhỏ nhất, các cái khác về `PENDING` (log `WARN`).

## 3. Log hoạt động — `logs/server.log`

### 3.1 Định dạng
```
[YYYY-MM-DD HH:MM:SS] [LEVEL] <subject> <ACTION> [args] [OK|REJECT(<code>)]
```
- `LEVEL` in đủ 5 ký tự: `INFO `, `WARN `, `ERROR`.
- `subject`: `session_subject(s)` (username nếu đã đăng nhập, ngược lại `ip:port`); sự kiện hệ thống dùng `server` hoặc `room=<id>`.
- Lệnh `OK` → `INFO`; `REJECT(4xx)` → `WARN`; `500` → `ERROR`.
- Mở chế độ `"a"`, `fflush` sau mỗi dòng.

### 3.2 Danh mục sự kiện bắt buộc

| ACTION | Level | Ví dụ |
|---|---|---|
| `START` / `STOP` | INFO | `[..] [INFO ] server START port=5500` |
| `CONNECT` | INFO | `[..] [INFO ] 127.0.0.1:52344 CONNECT` |
| `DISCONNECT` | INFO | `[..] [INFO ] alice DISCONNECT peer closed` |
| `BUSY` | WARN | `[..] [WARN ] 127.0.0.1:52399 BUSY REJECT(503)` |
| `LINE_TOO_LONG` | WARN | `[..] [WARN ] 127.0.0.1:52344 LINE_TOO_LONG REJECT(400)` |
| `<COMMAND>` | INFO/WARN | `[..] [INFO ] alice BID 12 600000 OK` · `[..] [WARN ] bob BID 12 605000 REJECT(422)` |
| `REGISTER`/`LOGIN` | INFO/WARN | `[..] [INFO ] 127.0.0.1:52344 LOGIN alice OK` (không bao giờ có mật khẩu) |
| `AUCTION_START` | INFO | `[..] [INFO ] room=1 AUCTION_START item=12` |
| `COUNTDOWN` | INFO | `[..] [INFO ] room=1 COUNTDOWN item=12 stage=2 price=600000` |
| `SOLD` / `UNSOLD` | INFO | `[..] [INFO ] room=1 SOLD item=12 to=carol price=600000` |
| `STORAGE` | ERROR | `[..] [ERROR] server STORAGE write data/items.txt failed: No space left on device` |
| `IO` | ERROR | `[..] [ERROR] alice IO recv: Connection reset by peer` |
