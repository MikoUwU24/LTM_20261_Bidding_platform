# 05 — Client Specification (`client/`)

> **Phụ thuộc:** spec 02 (giao thức), `common/stream`, `common/parse`. Client không chấm điểm riêng nhưng là công cụ demo chính.

---

## 1. Chạy

```bash
./bin/client <server_ip|hostname> <port>
```
- Địa chỉ: `inet_aton`; thất bại → `gethostbyname` (Ch3 s19); vẫn thất bại → báo lỗi, thoát 1.
- `socket` → `connect`; lỗi → `perror`, thoát 1. Socket để **chặn** (client chỉ có một kết nối); gửi bằng `stream_send_all`.

## 2. Vòng lặp

```text
loop:
    FD_ZERO(r); FD_SET(0, r); FD_SET(sock, r)
    select(sock+1, &r, NULL, NULL, NULL)            (EINTR → continue)
    if FD_ISSET(sock): recv → stream_recv_append → lặp stream_next_line(..., MAX_RESP_LINE) → ui_print_server_line
                        recv == 0 → "Server closed connection", thoát 0
    if FD_ISSET(0):    fgets dòng → ui_translate_input → nếu có lệnh: stream_send_all(line + "\r\n")
                        EOF (Ctrl+D) → close, thoát 0
```

## 3. Nhập liệu (`client/ui.c`)

| Người dùng gõ | Hành vi |
|---|---|
| `help` | In bảng lệnh của spec 02 §3 kèm ví dụ (không gửi) |
| `quit` | Đóng socket, thoát 0 (không gửi) |
| Dòng rỗng | Bỏ qua |
| Mọi dòng khác | Gửi nguyên văn (người dùng gõ lệnh giao thức, ví dụ `BID 12 600000`) |

Client **không** tự kiểm tra quyền/giá — server là nguồn quyết định.

## 4. Hiển thị (`client/ui.c`)

| Dòng nhận | Hiển thị |
|---|---|
| `2xx ...` | `[OK] <message>`; riêng `200 ROOMS`/`200 ITEMS` → in bảng (tách bằng `parse_split_records` + `parse_tokenize`) |
| `4xx/5xx ...` | `[ERR <code>] <message>` |
| `NOTIFY AUCTION_START ...` | `>> Đấu giá: #<id> <name> — <desc> | khởi điểm <start> | mua ngay <buy_now>` |
| `NOTIFY NEW_BID ...` | `>> Giá mới #<id>: <price> bởi <user>` |
| `NOTIFY COUNTDOWN ...` | `>> Đếm ngược lần <n>/3 #<id> — giá <price>` |
| `NOTIFY SOLD ...` / `UNSOLD` | `>> ĐÃ BÁN #<id> cho <user> giá <price>` / `>> KHÔNG BÁN ĐƯỢC #<id>` |
| `NOTIFY` khác | `>> <dòng gốc bỏ tiền tố NOTIFY>` |
| Không nhận dạng | In nguyên văn |

Giá in có dấu chấm phân cách nghìn (`600.000`). In prompt `> ` lại sau mỗi dòng server để không lẫn với nhập liệu.

## 5. Header

```c
/* client/ui.h */
void ui_print_help(void);
void ui_print_server_line(const char *line);     /* xử lý một dòng đã tách */
/* Trả 1 = gửi `line`, 0 = không gửi (help/rỗng), -1 = thoát (quit). */
int  ui_translate_input(char *line);
void ui_prompt(void);
```
