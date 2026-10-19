# AGENTS.md

Hướng dẫn cho AI coding agent (Claude Code, Codex, Copilot, …) khi làm việc trong repo này. Đọc file này **trước** khi sửa code. Đặc tả đầy đủ nằm trong [README.md](README.md). Nếu README và code mâu thuẫn, coi **README là nguồn chuẩn** và báo lại cho người dùng.

## Bối cảnh dự án

- Bài tập lớn môn **Lập trình mạng** (SoICT – HUST): hệ thống đấu giá trực tuyến client/server qua **TCP socket**.
- Ngôn ngữ: **C (C11)**, chạy trên **Linux / WSL**, build bằng `gcc` + `make`.
- Kiến thức nền nằm trong `docs/Chapter 1–7.pptx` (socket, TCP/UDP, fork, pthread, `select()`). Bám theo các API và kỹ thuật trong slide, **không** dùng framework mạng bên ngoài (libevent, Boost.Asio, …).
- Code được chấm theo 13 tiêu chí ở [README §10](README.md#10-bảng-đối-chiếu-tiêu-chí-chấm-điểm). Mỗi thay đổi nên gắn được với một tiêu chí cụ thể.

## Lệnh thường dùng

```bash
make                              # build ./bin/server và ./bin/client
make clean                        # xóa bin/ và file object
./bin/server 5500                 # chạy server
./bin/client 127.0.0.1 5500       # chạy client
nc -C 127.0.0.1 5500              # thử giao thức thủ công (-C gửi \r\n)
```

Build phải sạch với `-Wall -Wextra -Werror`. Trước khi báo hoàn thành, chạy `make` và một kịch bản thử bằng `nc` hoặc client cho chức năng vừa sửa.

## Cấu trúc thư mục

| Đường dẫn | Trách nhiệm |
|-----------|-------------|
| `common/protocol.h` | Tên lệnh, mã phản hồi, hằng số (`MAX_LINE`, `BID_STEP`, `AUCTION_IDLE_SEC`, `COUNTDOWN_INTERVAL_SEC`, …) |
| `common/stream.c/.h` | Bộ đệm nhận/gửi, tách dòng theo `\r\n` — dùng chung cho server và client |
| `server/main.c` | Khởi tạo socket, vòng lặp `select()`, tính timeout cho bộ đếm ngược |
| `server/session.c` | Ánh xạ `fd` ↔ phiên (trạng thái, user, phòng) |
| `server/account.c` | Đăng ký, đăng nhập, hash mật khẩu |
| `server/room.c` | Tạo/liệt kê/vào/rời phòng, broadcast trong phòng |
| `server/item.c` | Đăng bán, rút vật phẩm, hàng đợi FIFO |
| `server/auction.c` | Tố giá, mua ngay, bộ đếm ngược |
| `server/handler.c` | Phân tích lệnh, kiểm tra quyền, gọi module nghiệp vụ |
| `server/storage.c` | Đọc/ghi `data/*.txt` |
| `server/logger.c` | Ghi `logs/server.log` |
| `client/` | CLI client, `select()` trên `stdin` + socket |

`data/`, `logs/`, `bin/` và file `*.o` là sản phẩm lúc chạy/build — không commit (đưa vào `.gitignore`).

## Quy tắc kiến trúc (bắt buộc)

1. **Server là một tiến trình, một luồng, dùng `select()`.** Không thêm `fork()`, `pthread_create()` hay `SIGALRM`. Toàn bộ trạng thái dùng chung nằm trong một luồng nên không cần mutex — đừng phá vỡ giả định này.
2. **Socket client luôn non-blocking** (`fcntl(fd, F_SETFL, O_NONBLOCK)`). Không gọi `recv()`/`send()` chặn trong server.
3. **Không gọi `send()` trực tiếp trong handler.** Mọi phản hồi/thông báo đi qua hàng đợi gửi của session (vd. `session_send(s, fmt, ...)`); vòng lặp chính gửi khi `select()` báo socket ghi được và xử lý gửi một phần.
4. **Không giả định một `recv()` = một thông điệp.** Luôn nối vào bộ đệm và tách theo `\r\n` qua `common/stream.c`. Dòng vượt `MAX_LINE` → phản hồi `400` và đóng kết nối.
5. **Timer chỉ chạy qua timeout của `select()`.** Mỗi phòng giữ `deadline` và `countdown_stage`; `main.c` lấy `deadline` nhỏ nhất làm timeout và gọi `auction_tick(now)` sau mỗi vòng lặp.
6. `signal(SIGPIPE, SIG_IGN)` khi khởi động; kiểm tra `EINTR`, `EWOULDBLOCK`/`EAGAIN` ở mọi lời gọi I/O.
7. Khi đóng kết nối, luôn đi qua **một hàm duy nhất** (vd. `session_close()`): rời phòng → broadcast `USER_LEFT` → hủy phiên → `close(fd)` → ghi log.

## Giao thức

Đặc tả đầy đủ ở [README §5](README.md#5-giao-thức-ứng-dụng). Khi sửa giao thức:

- Lệnh viết HOA, tham số cách nhau bởi dấu cách, chuỗi có khoảng trắng nằm trong `"..."`.
- Mỗi yêu cầu nhận **đúng một** dòng `<CODE> <message>`. Sự kiện bất đồng bộ luôn bắt đầu bằng `NOTIFY `.
- Dùng đúng bảng mã `200/201/400/401/403/404/409/422/500/503`; không tự thêm mã mới khi chưa cập nhật README.
- Mọi lệnh/mã/sự kiện mới phải được khai báo trong `common/protocol.h` **và** cập nhật bảng trong README cùng lúc.

## Quy tắc nghiệp vụ dễ sai

- Bước giá tối thiểu **10.000 đồng**: `price >= current_price + BID_STEP`, trong đó `current_price = start_price` khi chưa có ai trả giá.
- `buy_now_price > start_price` khi đăng bán; `BID` với `price >= buy_now_price` được xử lý như `BUY_NOW`.
- Mỗi người **chỉ ở 1 phòng**; `JOIN_ROOM` khi đang ở phòng khác → `409`. `CREATE_ROOM` đưa người tạo vào phòng luôn (và cũng phải tuân thủ quy tắc này).
- Chỉ **chủ phòng** được `ADD_ITEM`/`REMOVE_ITEM`; chủ phòng **không** được `BID`/`BUY_NOW` trong phòng mình.
- `REMOVE_ITEM` chỉ hợp lệ khi vật phẩm ở trạng thái `PENDING`.
- Mỗi phòng có tối đa **một** vật phẩm `ON_AUCTION`; khi kết thúc phải tự động đưa vật phẩm `PENDING` kế tiếp lên.
- Đếm ngược: 180 giây không có giá mới → 3 lần `NOTIFY COUNTDOWN`, mỗi lần cách `COUNTDOWN_INTERVAL_SEC` (10 giây, giả định do đề bị cắt) → `SOLD`/`UNSOLD`. Một `BID` hợp lệ **reset** `countdown_stage = 0` và `deadline = now + 180`.
- Một tài khoản chỉ đăng nhập trên một kết nối; lần đăng nhập thứ hai → `409`.
- Người dùng mất kết nối: giá đã đặt vẫn giữ hiệu lực.

Thứ tự kiểm tra quyền trong `handler.c`: đăng nhập → ở trong phòng → vai trò (owner/member) → vật phẩm thuộc phòng hiện tại → kiểm tra giá trị. Giữ đúng thứ tự để mã lỗi nhất quán.

## Lưu trữ và log

- Dữ liệu ở `data/accounts.txt`, `data/rooms.txt`, `data/items.txt`, mỗi dòng một bản ghi (định dạng ở [README §7](README.md#7-lưu-trữ-dữ-liệu-và-ghi-log)). Ghi file qua file tạm rồi `rename()` để không hỏng dữ liệu khi crash.
- **Không bao giờ lưu mật khẩu dạng rõ** — lưu hash có salt.
- Mọi hành động thay đổi trạng thái và mọi lệnh bị từ chối đều phải gọi logger:
  `[YYYY-MM-DD HH:MM:SS] [LEVEL] <ip:port|username> <ACTION> <args> <OK|REJECT(code)>`.
- Không ghi mật khẩu vào log.

## Phong cách code

- C11, thụt lề 4 dấu cách, `snake_case` cho hàm/biến, `UPPER_SNAKE_CASE` cho hằng số và macro.
- Hàm public có tiền tố theo module: `room_create()`, `auction_bid()`, `session_send()`, …
- Không dùng magic number — khai báo trong `common/protocol.h`.
- Luôn kiểm tra giá trị trả về của system call; báo lỗi bằng `perror()` hoặc logger.
- Dùng `snprintf`/`strncpy` có giới hạn; không dùng `gets`, `sprintf`, `strcpy` với dữ liệu từ mạng.
- Mọi dữ liệu nhận từ client là không tin cậy: kiểm tra độ dài, số tham số, số âm, tràn số khi parse giá (`strtoll` + kiểm tra `errno`).
- Comment ngắn gọn, bằng tiếng Việt hoặc tiếng Anh, chỉ giải thích *vì sao* khi không hiển nhiên.

## Kiểm thử

- Mỗi chức năng cần ít nhất một kịch bản thử thủ công với 2–3 client cùng lúc (vd. hai người tố giá xen kẽ, một người rớt mạng giữa chừng).
- Kiểm thử truyền dòng: gửi một lệnh tách làm nhiều lần, và nhiều lệnh dính nhau trong một lần gửi, ví dụ:
  `printf 'LOG' | nc ...` rồi `printf 'IN a b\r\nLIST_ROOMS\r\n' | nc ...`.
- Khi kiểm thử đếm ngược, có thể tạm giảm `AUCTION_IDLE_SEC`/`COUNTDOWN_INTERVAL_SEC` qua `-D` khi build (`make CFLAGS_EXTRA="-DAUCTION_IDLE_SEC=10"`), **không** sửa giá trị mặc định trong code.

## Git

- Nhánh phát triển: `dev`; nhánh chính: `main`. Không commit thẳng lên `main`.
- Commit nhỏ, mỗi commit một chức năng/tiêu chí; message dạng `feat(auction): ...`, `fix(stream): ...`, `docs: ...`.
- Không commit `bin/`, `data/`, `logs/`, `*.o`.
- Không sửa nội dung `docs/` (slide bài giảng gốc).
