# 02 — Ranh giới kiến thức (chỉ dùng nội dung slide)

> **Luật:** Code chỉ dùng kỹ thuật/API có trong `docs/Chapter 1–7.pptx` (cột "Nguồn") hoặc trong danh sách **ngoại lệ có lý do** ở §3. Gặp nhu cầu ngoài hai danh sách → **dừng, báo người điều phối**, không tự thêm.
> Thư viện chuẩn C (`stdio.h`, `stdlib.h`, `string.h`, `ctype.h`, `stdarg.h`, `errno.h`, `time.h`, `limits.h`) là nền tảng ngôn ngữ, được dùng bình thường.

---

## 1. Tóm tắt nội dung slide

| Chương | Nội dung chính dùng cho dự án |
|---|---|
| Ch1 — Network Review | Mô hình client/server; "server manages login process" (s26); socket là IPC (s27–29) |
| Ch2 — C/C++ Basics | `gcc -c`, biên dịch nhiều file (s4–6); file I/O `fopen/fprintf/fscanf/fgets/fflush/fclose` (s20); `open/read/write/close` (s21); chuỗi `strncpy/strncmp/strchr/strstr/strtok_r/strerror` (s22–23); `memchr/memcpy/memmove/memset` (s24); `access/stat` (s25); `remove/rename` (s27) |
| Ch3 — Socket Introduction | Stream socket TCP, byte-stream đáng tin cậy (s6–7); `socket/bind/accept/connect/send/recv/close` (s9–10); 3 kiểu server (s9); `sockaddr_in` (s15); `htons/htonl/ntohs/ntohl` (s25–26); `inet_ntoa/inet_aton/inet_addr` (s27–29); `gethostbyname` (s19) |
| Ch4 — Basic TCP/UDP | Chữ ký và giá trị trả về `socket/connect/send/recv/close/bind/listen/accept` (s6–17); server nhận lệnh văn bản, thực thi, trả kết quả, **test bằng Netcat** (s20); `setsockopt` (s31) |
| Ch5 — Multi-Process | `fork`, `SIGCHLD`, `wait/waitpid`, `signal()` (s7–24) — **chỉ dùng `signal()`**, không fork |
| Ch6 — Multi-Thread | `pthread_*`, mutex — **không dùng** (ADR-01) |
| Ch7 — I/O Multiplexing | 5 mô hình I/O, **non-blocking trả `EWOULDBLOCK`** (s6), `select()` (s11–14), `struct timeval` & 3 chế độ timeout (s12, s14), `FD_ZERO/FD_SET/FD_CLR/FD_ISSET` (s15), chat-room server bằng `select` (s16), `FD_SETSIZE` (s17) |

---

## 2. Danh sách API được phép (whitelist)

| Mục đích | API | Nguồn |
|---|---|---|
| Tạo/đóng socket TCP | `socket(AF_INET, SOCK_STREAM, 0)`, `close()` | Ch3 s9–10, Ch4 s6–7, s11 |
| Server | `bind()`, `listen()`, `accept()` | Ch4 s15–17 |
| Client | `connect()` | Ch4 s8 |
| Truyền dữ liệu | `send()`, `recv()` (flags = 0) | Ch4 s9–10 |
| Địa chỉ | `struct sockaddr_in`, `htons/ntohs/htonl`, `INADDR_ANY`, `inet_ntoa`, `inet_aton` | Ch3 s15, s25–29 |
| Phân giải tên (client) | `gethostbyname()` khi `inet_aton` thất bại | Ch3 s19 |
| Đa hợp I/O | `select()`, `fd_set`, `FD_*`, `struct timeval`, `FD_SETSIZE` | Ch7 s11–17 |
| Tín hiệu | `signal(SIGPIPE, SIG_IGN)` | Ch5 (`signal()`) |
| File | `fopen/fgets/fprintf/fflush/fclose`, `rename()`, `remove()`, `access()` | Ch2 s20, s25, s27 |
| Chuỗi/bộ nhớ | `strncpy`, `strncmp`, `strcmp`, `strchr`, `strlen`, `strerror`, `memchr`, `memcpy`, `memmove`, `memset` | Ch2 s22–24 |
| Biên dịch | `gcc -c` từng file, link nhiều `.o` | Ch2 s4–6 |
| Kiểm thử | `nc` (netcat) | Ch4 s20 |

## 3. Ngoại lệ có lý do (được phép, dùng tối thiểu)

| API | Lý do | Gắn với slide |
|---|---|---|
| `fcntl(fd, F_GETFL)` + `fcntl(fd, F_SETFL, fl \| O_NONBLOCK)` | Cách duy nhất bật mô hình non-blocking I/O | Ch7 s6 (mô hình), Ch3 s5 (socket là file I/O Unix) |
| `errno`, `EINTR`, `EAGAIN`/`EWOULDBLOCK` | Phân biệt "chưa sẵn sàng" với lỗi thật | Ch7 s6 (`EWOULDBLOCK`), Ch4 s11 (`errno`) |
| `setsockopt(SOL_SOCKET, SO_REUSEADDR)` | Khởi động lại server ngay trên cùng cổng | Ch4 s31 (cùng hàm `setsockopt`, khác option) |
| `time()`, `localtime()`, `strftime()` | Deadline bộ đếm và dấu thời gian log | Thư viện chuẩn C |
| `snprintf`, `vsnprintf`, `va_list` | Định dạng chuỗi có giới hạn (thay `sprintf`) | Thư viện chuẩn C |
| `strtoll` + `errno == ERANGE` | Parse giá an toàn, chống tràn số | Thư viện chuẩn C |
| `rand()`, `srand()`, `getpid()` | Sinh salt (ADR-06) | Thư viện chuẩn C / Ch5 (process id) |
| Hàm băm FNV-1a tự viết | Không lưu mật khẩu rõ mà không thêm thư viện | ADR-06 |

## 4. Cấm dùng

`fork`, `wait*`, `pthread_*`, mutex, `SIGALRM`, `alarm`, `poll`, `epoll`, `SIGIO`/signal-driven I/O, `aio_*`, UDP (`sendto/recvfrom`), broadcast, `getaddrinfo`, `sprintf`, `strcpy`/`strcat`/`gets` trên dữ liệu mạng, mọi thư viện ngoài (OpenSSL, libcrypt, libevent, cJSON…), C++ STL.
