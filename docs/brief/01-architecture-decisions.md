# 01 — Architecture Decision Records (ADR) & Luật bất biến

> Mỗi ADR là luật. Muốn đổi → viết ADR mới thay thế, không sửa âm thầm.

---

## ADR-01: Concurrent single server bằng `select()` (một tiến trình, một luồng)

- **Bối cảnh:** Slide trình bày 3 kiểu server: iterating, forking (Ch5), concurrent single server với `select` (Ch3 s9, Ch4 s13, Ch7). Đấu giá cần chia sẻ trạng thái (phòng, giá) và broadcast cho cả phòng.
- **Quyết định:** Dùng `select()`; **cấm** `fork()`, `pthread_create()`, `SIGALRM`, `poll`, signal-driven I/O.
- **Hệ quả:** Không race condition, không mutex; hai `BID` gần như đồng thời được xử lý tuần tự theo thứ tự đến. Giới hạn kết nối bởi `FD_SETSIZE` (Ch7 s17) → vượt thì trả `503`.

## ADR-02: Giao thức dòng văn bản, đóng khung bằng `\r\n`, bộ đệm riêng mỗi phiên

- **Bối cảnh:** TCP là byte-stream (Ch3 s6): một `recv()` có thể chứa nửa dòng hoặc nhiều dòng; `send()` có thể gửi thiếu.
- **Quyết định:**
  1. Mỗi thông điệp là một dòng kết thúc `\r\n`; dòng request dài tối đa `MAX_LINE` byte (tính cả `\r\n`).
  2. Mỗi phiên có `recv_buffer_t` (nối → tách dòng → `memmove` phần dư) và `send_buffer_t` (hàng đợi gửi, gửi khi `select()` báo ghi được).
  3. Server **dung thứ** dòng kết thúc chỉ bằng `\n` (bỏ `\r` cuối nếu có) để `nc` không `-C` vẫn dùng được; server **luôn gửi** `\r\n`.
- **Hệ quả:** Handler không bao giờ gọi `send()`; mọi output qua `session_send()`.

## ADR-03: Bộ đếm ngược chỉ bằng timeout của `select()`

- **Quyết định:** Mỗi `room_t` giữ `deadline` (`time_t`, 0 = không chạy) và `countdown_stage` (0..3). Trước mỗi `select()`, `main.c` lấy `deadline` nhỏ nhất làm `struct timeval` timeout (Ch7 s12–14); sau `select()` gọi `auction_tick(time(NULL))`.
- **Lịch thời gian** (T = lúc vật phẩm lên sàn hoặc lúc `BID` hợp lệ gần nhất):

  | Thời điểm | Hành động |
  |---|---|
  | T + 180s | `COUNTDOWN 1` |
  | T + 190s | `COUNTDOWN 2` |
  | T + 200s | `COUNTDOWN 3` |
  | T + 210s | `SOLD` (có `winner`) hoặc `UNSOLD`, đưa vật phẩm kế tiếp lên |

- `BID` hợp lệ ở bất kỳ giai đoạn nào → `countdown_stage = 0`, `deadline = now + AUCTION_IDLE_SEC`.
- Hằng số có `#ifndef` để test build với `-DAUCTION_IDLE_SEC=3 -DCOUNTDOWN_INTERVAL_SEC=1`; **không** sửa mặc định trong code.

## ADR-04: Phản hồi trước, thông báo sau — một điểm gửi phản hồi và ghi log

- **Bối cảnh:** README yêu cầu "mỗi yêu cầu nhận đúng một dòng phản hồi"; ví dụ §5.5 cho thấy `200 Bid accepted` đến **trước** `NOTIFY NEW_BID`.
- **Quyết định:**
  1. Mỗi hàm lệnh `cmd_xxx()` gọi `cmd_reply(s, code, fmt, ...)` **đúng một lần**, **trước** mọi `session_broadcast_room()` / `session_send()` khác cho chính phiên đó.
  2. `cmd_reply()` vừa xếp hàng phản hồi vừa ghi log `OK`/`REJECT(code)` — không chỗ nào khác log kết quả lệnh.
  3. `handler.c` kiểm tra cờ `s->replied`; nếu hàm lệnh quên reply → tự gửi `500` và log `ERROR`.

## ADR-05: Lưu trữ file văn bản, bộ nhớ là bản chính, ghi đè qua file tạm + `rename()`

- **Quyết định:** Nạp toàn bộ `data/*.txt` lúc khởi động vào mảng tĩnh; sau mỗi thay đổi trạng thái, ghi lại **cả file** vào `<file>.tmp` rồi `rename()` (Ch2 s27). Dữ liệu nhỏ nên chi phí chấp nhận được.
- `storage.c` chỉ cung cấp hàm đọc/ghi dòng chung; **định dạng bản ghi do từng module** (`account/room/item`) tự viết → các agent làm song song không đụng nhau.

## ADR-06: Băm mật khẩu tự cài đặt, có salt (không thư viện ngoài)

- **Bối cảnh:** Slide không dạy thư viện mã hoá; không được thêm OpenSSL/libcrypt. README cấm lưu mật khẩu rõ.
- **Quyết định:** `hash = FNV-1a 64-bit` lặp `HASH_ROUNDS` vòng trên chuỗi `salt + password`; `salt` = `SALT_LEN` ký tự hex sinh bằng `rand()` (seed `time(NULL) ^ getpid()` một lần lúc khởi động). Lưu `username salt hash_hex`.
- **Hệ quả:** Không lưu/log mật khẩu rõ. Ghi chú trong báo cáo: thuật toán đủ cho bài tập, không đạt chuẩn an toàn thực tế.

## ADR-07: Đóng băng interface (header) trước khi chạy song song

- **Quyết định:** Chặng 0 do người điều phối viết `common/protocol.h` và **mọi `*.h`** với chữ ký hàm đúng `specs/03`, cùng file `.c` stub biên dịch được. Agent các chặng sau **chỉ sửa `.c` được giao**; cần đổi header → dừng và báo.
- **Hệ quả:** `make` luôn xanh ở mọi nhánh; merge ít xung đột.

## ADR-08: Kiểm thử bằng `nc` + script bash, unit test C thuần

- **Bối cảnh:** Ch4 s20 "Use Netcat for testing".
- **Quyết định:** `tests/lib.sh` mở nhiều client `nc` qua FIFO; `tests/at_*.sh` mỗi file một nhóm AT ở `plans/02`; `tests/unit/*.c` dùng `assert` cho `stream`, `parse`, hash. `make test` chạy tất cả với timer rút ngắn.

## ADR-09: Thứ tự kiểm tra lệnh cố định

Áp dụng trong `handler.c`, theo đúng thứ tự, dừng ở lỗi đầu tiên:

| Bước | Kiểm tra | Mã |
|:-:|---|:-:|
| 1 | Lệnh có trong bảng? | `400` |
| 2 | Guard đăng nhập (`G_AUTH`) / chưa đăng nhập (`G_NOT_AUTH`) | `401` / `409` |
| 3 | Guard ở trong phòng (`G_IN_ROOM`) / chưa ở phòng (`G_NOT_IN_ROOM`) | `403` / `409` |
| 4 | Guard vai trò (`G_OWNER` / `G_NOT_OWNER`) | `403` |
| 5 | Số tham số, định dạng tham số (độ dài, ký tự, số) | `400` |
| 6 | Đối tượng tồn tại và thuộc phòng hiện tại | `404` |
| 7 | Trạng thái đối tượng (vd. vật phẩm không `PENDING`, không `ON_AUCTION`) | `409` |
| 8 | Giá trị nghiệp vụ (giá) | `422` |

Bước 1–5 do `handler.c` làm chung; bước 6–8 trong hàm `cmd_xxx` của module.

## ADR-10: Khởi động lại server

- Tài khoản, phòng, vật phẩm (kể cả `current_price`, `winner`) được nạp lại; không phiên nào tồn tại.
- Phòng có vật phẩm `ON_AUCTION` → bộ đếm khởi động lại từ đầu (`stage = 0`, `deadline = now + AUCTION_IDLE_SEC`).
- Phòng không có `ON_AUCTION` nhưng còn `PENDING` → đưa vật phẩm `PENDING` đầu tiên lên.
- `next_room_id`, `next_item_id` = id lớn nhất trong file + 1.
