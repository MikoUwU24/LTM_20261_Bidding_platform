# Bidding Platform — Hệ thống đấu giá trực tuyến qua TCP Socket

Bài tập lớn môn **Lập trình mạng** (Network Programming) — Trường CNTT&TT, ĐHBK Hà Nội.

Hệ thống client/server cho phép nhiều người dùng cùng tham gia các phòng đấu giá theo thời gian thực. Server và client được viết bằng **C/C++ trên Linux**, giao tiếp qua **TCP stream socket** bằng một **giao thức dạng văn bản (text-based) tự định nghĩa**.

---

## Mục lục

1. [Chức năng](#1-chức-năng)
2. [Kiến trúc tổng quan](#2-kiến-trúc-tổng-quan)
3. [Xử lý truyền dòng](#3-xử-lý-truyền-dòng-tcp-stream)
4. [Cơ chế vào/ra socket trên server](#4-cơ-chế-vàora-socket-trên-server)
5. [Giao thức ứng dụng](#5-giao-thức-ứng-dụng)
6. [Luồng nghiệp vụ chi tiết](#6-luồng-nghiệp-vụ-chi-tiết)
7. [Lưu trữ dữ liệu và ghi log](#7-lưu-trữ-dữ-liệu-và-ghi-log)
8. [Cấu trúc thư mục](#8-cấu-trúc-thư-mục)
9. [Biên dịch và chạy](#9-biên-dịch-và-chạy)
10. [Bảng đối chiếu tiêu chí chấm điểm](#10-bảng-đối-chiếu-tiêu-chí-chấm-điểm)
11. [Tài liệu tham khảo](#11-tài-liệu-tham-khảo)

---

## 1. Chức năng

### 1.1. Tài khoản và phiên làm việc

- **Đăng ký** tài khoản mới (username duy nhất, mật khẩu).
- **Đăng nhập / đăng xuất**. Một tài khoản chỉ được đăng nhập trên **một** kết nối tại một thời điểm.
- Server quản lý **phiên (session)** cho mỗi kết nối: chưa đăng nhập → đã đăng nhập → đang ở trong phòng.
- Khi client mất kết nối, server tự động rời phòng và hủy phiên của người dùng đó.

### 1.2. Phòng đấu giá

- **Tạo phòng đấu giá**: người tạo trở thành **chủ phòng (owner)**.
- **Liệt kê phòng đấu giá**: tên phòng, chủ phòng, số người đang tham gia, vật phẩm đang được đấu giá, số vật phẩm đang chờ.
- **Tham gia phòng**: tại một thời điểm, mỗi người dùng **chỉ được ở trong 1 phòng**. Muốn sang phòng khác phải rời phòng hiện tại.
- **Rời phòng**.

### 1.3. Vật phẩm

- **Đăng bán vật phẩm**: chỉ chủ phòng được đăng bán vật phẩm trong phòng do mình tạo. Mỗi vật phẩm gồm:
  - Tên, mô tả
  - Giá khởi điểm
  - Giá bán ngay (phải lớn hơn giá khởi điểm)
- Các vật phẩm được đưa vào **hàng đợi** của phòng và **lần lượt** được giới thiệu trong phiên bán (FIFO).
- **Rút vật phẩm**: chủ phòng có thể rút vật phẩm ra khỏi phòng **nếu vật phẩm đó chưa được đem ra đấu giá** (vẫn đang ở trạng thái chờ).

### 1.4. Đấu giá

- **Tố giá (bid)**: giá mới phải **lớn hơn giá hiện tại ít nhất 10.000 đồng**.
  - Nếu chưa có ai trả giá, giá hiện tại là giá khởi điểm.
  - Chủ phòng không được tự trả giá vật phẩm của mình.
- Khi lệnh đặt giá hợp lệ, **toàn bộ người dùng trong phòng** nhận được thông báo giá mới.
- **Đếm ngược kết thúc**: sau mỗi lần đặt giá **3 phút**, nếu không có ai trả giá cao hơn, server gửi **thông báo 3 lần**, mỗi lần cách nhau **10 giây**. Sau lần thông báo thứ 3 mà vẫn không có giá mới, vật phẩm được **bán cho người trả giá cao nhất**. Nếu trong thời gian đếm ngược có người trả giá mới, bộ đếm được khởi động lại từ đầu.
- Nếu hết thời gian mà không có ai trả giá, vật phẩm được đánh dấu **không bán được** và phòng chuyển sang vật phẩm kế tiếp.

> **Ghi chú:** đề bài gốc bị cắt ở đoạn "mỗi lần cách nh…". README này giả định khoảng cách giữa các lần thông báo là **10 giây**; giá trị này là một hằng số cấu hình (`COUNTDOWN_INTERVAL_SEC`) nên có thể đổi dễ dàng.

### 1.5. Mua ngay

- Người dùng trong phòng có thể **mua ngay** vật phẩm đang được đấu giá với **giá bán ngay**, không cần qua đấu giá.
- Phiên đấu giá của vật phẩm kết thúc ngay lập tức, cả phòng được thông báo, và phòng chuyển sang vật phẩm kế tiếp.
- Lệnh mua ngay chỉ hợp lệ khi giá hiện tại vẫn còn thấp hơn giá bán ngay.

### 1.6. Ghi log hoạt động

- Mọi sự kiện quan trọng (kết nối, đăng nhập, tạo phòng, đặt giá, bán vật phẩm, lỗi, …) được ghi vào file log kèm thời gian và địa chỉ client.

---

## 2. Kiến trúc tổng quan

Hệ thống theo mô hình **Client/Server**: một server trung tâm quản lý tài khoản, phòng và vật phẩm; nhiều client kết nối đến server qua TCP.

```
 ┌──────────┐                                  ┌───────────────────────────────────────────┐
 │ Client A │──┐                               │                 SERVER                    │
 └──────────┘  │        TCP (port 5500)        │                                           │
 ┌──────────┐  │   ┌───────────────────────┐   │  ┌─────────────┐   ┌──────────────────┐   │
 │ Client B │──┼──▶│ "CMD arg1 arg2\r\n"   │──▶│  │ I/O layer   │──▶│ Command handler  │   │
 └──────────┘  │   │ "200 OK ...\r\n"      │◀──│  │ select()    │   │ (auth/room/item/ │   │
 ┌──────────┐  │   │ "NOTIFY ...\r\n"      │   │  │ + buffers   │   │  bid/buy)        │   │
 │ Client C │──┘   └───────────────────────┘   │  └─────────────┘   └────────┬─────────┘   │
 └──────────┘                                  │         ▲                   │             │
                                               │         │ timeout     ┌─────▼──────┐      │
                                               │  ┌──────┴──────┐      │ Data store │      │
                                               │  │ Timer       │      │ (files)    │      │
                                               │  │ (countdown) │      └────────────┘      │
                                               │  └─────────────┘      ┌────────────┐      │
                                               │                       │ Logger     │      │
                                               │                       └────────────┘      │
                                               └───────────────────────────────────────────┘
```

| Thành phần        | Vai trò |
|-------------------|---------|
| **I/O layer**     | Chấp nhận kết nối, đọc/ghi socket không chặn bằng `select()`, tách luồng byte thành các thông điệp. |
| **Session manager** | Ánh xạ socket ↔ phiên làm việc (trạng thái đăng nhập, user, phòng đang tham gia). |
| **Command handler** | Phân tích lệnh, kiểm tra quyền, thực thi nghiệp vụ, sinh phản hồi và thông báo. |
| **Auction engine** | Quản lý hàng đợi vật phẩm, giá hiện tại, bộ đếm ngược 3 phút + 3 lần thông báo. |
| **Data store**    | Lưu tài khoản, phòng, vật phẩm, lịch sử giao dịch ra file. |
| **Logger**        | Ghi log hoạt động có dấu thời gian. |

---

## 3. Xử lý truyền dòng (TCP stream)

TCP là giao thức **hướng dòng byte**, không bảo toàn ranh giới thông điệp: một lần `recv()` có thể nhận **một phần** thông điệp, hoặc **nhiều** thông điệp dính nhau; `send()` cũng có thể chỉ gửi được một phần dữ liệu. Hệ thống xử lý như sau:

**Đóng khung thông điệp (framing):** mỗi thông điệp là một dòng văn bản kết thúc bằng `\r\n`. Độ dài tối đa một dòng là `MAX_LINE = 4096` byte; dòng dài hơn bị coi là lỗi giao thức.

**Phía nhận** — mỗi client có một bộ đệm nhận riêng:

```c
typedef struct {
    char   buf[RECV_BUF_SIZE];
    size_t len;              /* số byte đang có trong bộ đệm */
} recv_buffer_t;
```

1. `recv()` được gọi khi `select()` báo socket sẵn sàng đọc; dữ liệu được **nối thêm** vào cuối bộ đệm.
2. Duyệt bộ đệm, tìm `\r\n`. Mỗi lần tìm thấy → cắt ra một thông điệp hoàn chỉnh và đưa vào bộ xử lý lệnh.
3. Phần còn lại (thông điệp chưa đủ) được `memmove()` về đầu bộ đệm, chờ lần đọc tiếp theo.
4. `recv()` trả về `0` → client đóng kết nối; trả về `-1` (khác `EWOULDBLOCK`/`EINTR`) → lỗi, đóng kết nối.

**Phía gửi** — mỗi client có một hàng đợi gửi riêng:

1. Phản hồi và thông báo được **nối vào hàng đợi gửi** thay vì gọi `send()` trực tiếp.
2. Khi `select()` báo socket sẵn sàng ghi, server gọi `send()` và **dịch con trỏ** theo số byte thực sự đã gửi; phần chưa gửi giữ lại cho lần sau.
3. Nhờ đó server không bao giờ bị chặn bởi một client chậm.

Client cũng dùng cùng cơ chế đóng khung để tách các dòng phản hồi và các dòng thông báo `NOTIFY` đến bất đồng bộ.

---

## 4. Cơ chế vào/ra socket trên server

Server dùng mô hình **I/O multiplexing với `select()`** (Concurrent single server) — một tiến trình, một luồng phục vụ đồng thời nhiều client:

```
socket() → setsockopt(SO_REUSEADDR) → bind() → listen()
loop:
    FD_ZERO(readfds), FD_ZERO(writefds)
    FD_SET(listenfd, readfds)
    với mỗi client: FD_SET(fd, readfds); nếu hàng đợi gửi khác rỗng → FD_SET(fd, writefds)
    timeout = thời điểm hết hạn gần nhất của các bộ đếm ngược − now
    n = select(maxfd + 1, &readfds, &writefds, NULL, &timeout)
    nếu listenfd sẵn sàng      → accept(), fcntl(O_NONBLOCK), tạo session mới
    nếu client fd sẵn sàng đọc → recv() vào bộ đệm, tách và xử lý các dòng lệnh
    nếu client fd sẵn sàng ghi → send() phần còn lại trong hàng đợi gửi
    xử lý các bộ đếm ngược đã đến hạn (gửi NOTIFY COUNTDOWN / SOLD)
```

**Lý do chọn `select()` thay vì fork/thread:**

- Dữ liệu dùng chung (phòng, vật phẩm, giá hiện tại) được truy cập trong một luồng duy nhất → **không có race condition**, không cần mutex. Việc hai người cùng đặt giá gần như đồng thời được xử lý tuần tự đúng thứ tự đến.
- Gửi thông báo đến tất cả thành viên trong phòng rất đơn giản (cùng tiến trình, có sẵn danh sách socket). Với forking server, các tiến trình con không chia sẻ bộ nhớ nên việc broadcast phức tạp hơn nhiều.
- **Bộ đếm ngược** được tích hợp tự nhiên qua tham số `timeout` của `select()`, không cần thêm luồng hẹn giờ hay tín hiệu `SIGALRM`.

Các chi tiết khác:

- Bỏ qua `SIGPIPE` (`signal(SIGPIPE, SIG_IGN)`) để server không bị dừng khi ghi vào socket đã đóng.
- Giới hạn số kết nối đồng thời bởi `FD_SETSIZE`; kết nối vượt quá bị từ chối với mã `503`.
- Khi đóng kết nối: `close(fd)`, `FD_CLR`, rời phòng, hủy phiên, ghi log.

---

## 5. Giao thức ứng dụng

### 5.1. Định dạng chung

```
Yêu cầu  (client → server):  <COMMAND> [<arg1> <arg2> ...]\r\n
Phản hồi (server → client):  <CODE> <message>\r\n
Thông báo (server → client): NOTIFY <EVENT> [<arg> ...]\r\n
```

- Các tham số cách nhau bởi dấu cách. Tham số có thể chứa khoảng trắng (tên vật phẩm, mô tả) được đặt trong dấu ngoặc kép `"..."`.
- Giá tiền là số nguyên, đơn vị **đồng**.
- Mỗi yêu cầu nhận **đúng một** dòng phản hồi. Dòng `NOTIFY` có thể đến bất kỳ lúc nào, client phân biệt bằng tiền tố `NOTIFY`.

### 5.2. Danh sách lệnh

| Lệnh | Cú pháp | Điều kiện | Mô tả |
|------|---------|-----------|-------|
| Đăng ký | `REGISTER <username> <password>` | Chưa đăng nhập | Tạo tài khoản mới |
| Đăng nhập | `LOGIN <username> <password>` | Chưa đăng nhập | Tạo phiên |
| Đăng xuất | `LOGOUT` | Đã đăng nhập | Rời phòng (nếu có) và hủy phiên |
| Tạo phòng | `CREATE_ROOM "<room_name>"` | Đã đăng nhập | Tạo phòng, trả về `room_id` |
| Liệt kê phòng | `LIST_ROOMS` | Đã đăng nhập | Danh sách các phòng |
| Vào phòng | `JOIN_ROOM <room_id>` | Đã đăng nhập, chưa ở phòng nào | Tham gia phòng |
| Rời phòng | `LEAVE_ROOM` | Đang ở trong phòng | Rời phòng hiện tại |
| Xem vật phẩm | `LIST_ITEMS` | Đang ở trong phòng | Vật phẩm đang đấu giá và hàng đợi |
| Đăng bán | `ADD_ITEM "<name>" "<desc>" <start_price> <buy_now_price>` | Là chủ phòng, đang ở trong phòng | Thêm vật phẩm vào hàng đợi |
| Rút vật phẩm | `REMOVE_ITEM <item_id>` | Là chủ phòng, vật phẩm đang chờ | Xóa vật phẩm khỏi hàng đợi |
| Tố giá | `BID <item_id> <price>` | Ở trong phòng, không phải chủ phòng | Đặt giá cho vật phẩm đang đấu giá |
| Mua ngay | `BUY_NOW <item_id>` | Ở trong phòng, không phải chủ phòng | Mua với giá bán ngay |

### 5.3. Mã phản hồi

| Mã | Ý nghĩa |
|----|---------|
| `200` | Thành công |
| `201` | Tạo mới thành công (tài khoản, phòng, vật phẩm) |
| `400` | Sai cú pháp / tham số không hợp lệ |
| `401` | Chưa đăng nhập hoặc sai username/mật khẩu |
| `403` | Không có quyền (không phải chủ phòng, chủ phòng tự đặt giá, …) |
| `404` | Không tìm thấy phòng / vật phẩm |
| `409` | Xung đột trạng thái (username đã tồn tại, tài khoản đang đăng nhập ở nơi khác, đã ở trong phòng khác, vật phẩm đã lên sàn nên không rút được, …) |
| `422` | Giá không hợp lệ (thấp hơn giá hiện tại + 10.000, giá bán ngay ≤ giá khởi điểm, …) |
| `500` | Lỗi phía server |
| `503` | Server quá tải |

### 5.4. Thông báo bất đồng bộ (`NOTIFY`)

| Sự kiện | Cú pháp | Khi nào |
|---------|---------|---------|
| Có người vào/rời phòng | `NOTIFY USER_JOINED <username>` / `NOTIFY USER_LEFT <username>` | Thành viên thay đổi |
| Vật phẩm mới được thêm | `NOTIFY ITEM_ADDED <item_id> "<name>"` | Chủ phòng đăng bán |
| Vật phẩm bị rút | `NOTIFY ITEM_REMOVED <item_id>` | Chủ phòng rút vật phẩm |
| Bắt đầu đấu giá | `NOTIFY AUCTION_START <item_id> "<name>" "<desc>" <start_price> <buy_now_price>` | Vật phẩm kế tiếp được đưa lên |
| Giá mới | `NOTIFY NEW_BID <item_id> <username> <price>` | Có lệnh đặt giá hợp lệ |
| Đếm ngược | `NOTIFY COUNTDOWN <item_id> <lần 1/2/3> <price>` | Hết 3 phút không có giá mới |
| Đã bán | `NOTIFY SOLD <item_id> <username> <price>` | Kết thúc đấu giá / mua ngay |
| Không bán được | `NOTIFY UNSOLD <item_id>` | Hết thời gian mà không ai trả giá |

### 5.5. Ví dụ phiên làm việc

```
C: LOGIN alice 123456
S: 200 Login successful
C: JOIN_ROOM 3
S: 200 Joined room 3 "Do co"
S: NOTIFY AUCTION_START 12 "Binh gom" "Gom Bat Trang the ky 19" 500000 2000000
C: BID 12 505000
S: 422 Bid must be at least 510000
C: BID 12 600000
S: 200 Bid accepted
S: NOTIFY NEW_BID 12 alice 600000
   ... 3 phút không có ai trả giá ...
S: NOTIFY COUNTDOWN 12 1 600000
S: NOTIFY COUNTDOWN 12 2 600000
S: NOTIFY COUNTDOWN 12 3 600000
S: NOTIFY SOLD 12 alice 600000
```

---

## 6. Luồng nghiệp vụ chi tiết

### 6.1. Trạng thái phiên và kiểm soát quyền truy cập

```
            REGISTER / LOGIN thất bại
                   ┌────┐
                   ▼    │
   [CONNECTED] ────────┘
        │ LOGIN thành công
        ▼
   [AUTHENTICATED] ◀────────── LEAVE_ROOM ─────────┐
        │ JOIN_ROOM / CREATE_ROOM (tự vào phòng)   │
        ▼                                          │
   [IN_ROOM (member | owner)] ─────────────────────┘
        │ LOGOUT / mất kết nối
        ▼
   [CLOSED]
```

Mỗi lệnh được kiểm tra theo thứ tự trước khi thực thi:

1. **Đã đăng nhập chưa?** — mọi lệnh ngoài `REGISTER`, `LOGIN` đều yêu cầu đăng nhập (`401`).
2. **Có đang ở trong phòng không?** — `BID`, `BUY_NOW`, `LIST_ITEMS`, `ADD_ITEM`, `REMOVE_ITEM`, `LEAVE_ROOM` yêu cầu đang ở trong phòng (`403`).
3. **Vai trò trong phòng** — `ADD_ITEM`, `REMOVE_ITEM` chỉ dành cho chủ phòng; `BID`, `BUY_NOW` chỉ dành cho người không phải chủ phòng (`403`).
4. **Chỉ một phòng** — `JOIN_ROOM` khi đang ở phòng khác bị từ chối (`409`).
5. **Phạm vi vật phẩm** — `item_id` phải thuộc phòng mà người dùng đang ở (`404`).

### 6.2. Vòng đời vật phẩm

```
 ADD_ITEM        đến lượt          BID hợp lệ (reset timer)
 ───────▶ [PENDING] ───────▶ [ON_AUCTION] ◀──────┐
              │                   │  │  └────────┘
  REMOVE_ITEM │      3 phút +     │  │ BUY_NOW
              ▼      3×COUNTDOWN  │  │
         [REMOVED]                ▼  ▼
                        [SOLD] hoặc [UNSOLD]
```

- Mỗi phòng có tối đa **một** vật phẩm `ON_AUCTION` tại một thời điểm.
- Khi vật phẩm hiện tại kết thúc (`SOLD`/`UNSOLD`), vật phẩm `PENDING` đầu tiên trong hàng đợi tự động chuyển sang `ON_AUCTION`.
- Chỉ vật phẩm ở trạng thái `PENDING` mới được rút.

### 6.3. Bộ đếm ngược đấu giá

Mỗi phòng có một bộ đếm với các trường `deadline` và `countdown_stage` (0–3):

| Sự kiện | Hành động |
|---------|-----------|
| Vật phẩm bắt đầu đấu giá / có `BID` hợp lệ | `countdown_stage = 0`, `deadline = now + 180s` |
| Đến `deadline`, `stage < 3` | `stage++`, gửi `NOTIFY COUNTDOWN <stage>`, `deadline = now + 10s` |
| Đến `deadline`, `stage == 3` | Có người trả giá → `SOLD`; không ai trả giá → `UNSOLD`; chuyển vật phẩm kế tiếp |

Vòng lặp chính tính `timeout` cho `select()` bằng `deadline` nhỏ nhất của tất cả các phòng.

### 6.4. Kiểm tra lệnh tố giá

```
valid = item.status == ON_AUCTION
     && user != room.owner
     && price >= item.current_price + 10000      (current_price = start_price nếu chưa có giá)
```

Nếu `price >= buy_now_price`, lệnh được xử lý như **mua ngay** ở giá bán ngay.

### 6.5. Xử lý mất kết nối

- Người dùng thường: bị loại khỏi phòng, giá đã đặt **vẫn giữ hiệu lực**.
- Chủ phòng: phòng vẫn tiếp tục phiên đấu giá với các vật phẩm đã đăng; chủ phòng có thể đăng nhập lại và vào lại phòng của mình.

---

## 7. Lưu trữ dữ liệu và ghi log

Dữ liệu được lưu trong thư mục `data/` dưới dạng file văn bản, nạp vào bộ nhớ khi khởi động và ghi lại khi có thay đổi:

| File | Nội dung (mỗi dòng một bản ghi) |
|------|------|
| `data/accounts.txt` | `username password_hash` |
| `data/rooms.txt` | `room_id owner "room_name"` |
| `data/items.txt` | `item_id room_id status start_price buy_now_price current_price winner "name" "desc"` |

Mật khẩu được lưu dưới dạng **hash có salt**, không lưu bản rõ.

**Log hoạt động** được ghi vào `logs/server.log`:

```
[2026-10-12 14:03:21] [INFO ] 192.168.1.15:52344 CONNECT
[2026-10-12 14:03:25] [INFO ] 192.168.1.15:52344 LOGIN alice OK
[2026-10-12 14:03:30] [INFO ] alice JOIN_ROOM 3
[2026-10-12 14:04:02] [INFO ] alice BID item=12 price=600000 OK
[2026-10-12 14:04:05] [WARN ] bob BID item=12 price=605000 REJECT(422)
[2026-10-12 14:07:32] [INFO ] room=3 item=12 SOLD to alice price=600000
[2026-10-12 14:10:11] [INFO ] 192.168.1.15:52344 DISCONNECT
```

Mỗi dòng gồm: thời gian, mức độ (`INFO`/`WARN`/`ERROR`), chủ thể (địa chỉ client hoặc username), hành động, kết quả.

---

## 8. Cấu trúc thư mục

```
.
├── README.md
├── Makefile
├── docs/                 # Slide bài giảng tham khảo
├── common/
│   ├── protocol.h        # Mã lệnh, mã phản hồi, hằng số (MAX_LINE, BID_STEP, ...)
│   └── stream.c/.h       # Bộ đệm nhận/gửi, tách dòng theo \r\n
├── server/
│   ├── main.c            # Khởi tạo socket, vòng lặp select()
│   ├── session.c/.h      # Quản lý phiên, ánh xạ fd ↔ user
│   ├── account.c/.h      # Đăng ký, đăng nhập, hash mật khẩu
│   ├── room.c/.h         # Tạo/liệt kê/vào/rời phòng, broadcast trong phòng
│   ├── item.c/.h         # Đăng bán, rút vật phẩm, hàng đợi
│   ├── auction.c/.h      # Tố giá, mua ngay, bộ đếm ngược
│   ├── handler.c/.h      # Phân tích lệnh và kiểm tra quyền
│   ├── storage.c/.h      # Đọc/ghi file dữ liệu
│   └── logger.c/.h       # Ghi log
├── client/
│   ├── main.c            # Giao diện dòng lệnh, select() trên stdin + socket
│   └── ui.c/.h           # Hiển thị menu, phản hồi và thông báo
├── data/                 # Dữ liệu lưu trữ (tự tạo khi chạy)
└── logs/                 # File log (tự tạo khi chạy)
```

Client dùng `select()` đồng thời trên `stdin` và socket để vừa nhận lệnh từ bàn phím vừa hiển thị ngay các thông báo `NOTIFY` từ server.

---

## 9. Biên dịch và chạy

**Yêu cầu:** Linux (hoặc WSL), `gcc`, `make`.

```bash
# Biên dịch
make            # tạo ./bin/server và ./bin/client

# Chạy server
./bin/server <port>             # ví dụ: ./bin/server 5500

# Chạy client
./bin/client <server_ip> <port> # ví dụ: ./bin/client 127.0.0.1 5500
```

Có thể kiểm thử giao thức thủ công bằng `netcat`:

```bash
nc -C 127.0.0.1 5500     # -C: gửi \r\n ở cuối mỗi dòng
LOGIN alice 123456
LIST_ROOMS
```

---

## 10. Bảng đối chiếu tiêu chí chấm điểm

| # | Tiêu chí | Điểm | Thực hiện tại |
|---|----------|:----:|---------------|
| 1 | Xử lý truyền dòng | 1 | Mục [3](#3-xử-lý-truyền-dòng-tcp-stream): đóng khung `\r\n`, bộ đệm nhận/gửi theo từng client, xử lý nhận thiếu/dính gói, gửi một phần — `common/stream.c` |
| 2 | Cài đặt cơ chế vào/ra socket trên server | 2 | Mục [4](#4-cơ-chế-vàora-socket-trên-server): `select()` + socket không chặn, tích hợp timer — `server/main.c` |
| 3 | Đăng ký và quản lý tài khoản | 2 | `REGISTER`, kiểm tra trùng username, hash mật khẩu, lưu file — `server/account.c`, `server/storage.c` |
| 4 | Đăng nhập và quản lý phiên | 2 | `LOGIN`/`LOGOUT`, một tài khoản một phiên, hủy phiên khi mất kết nối — `server/session.c` |
| 5 | Kiểm soát quyền truy cập phòng đấu giá | 1 | Mục [6.1](#61-trạng-thái-phiên-và-kiểm-soát-quyền-truy-cập): kiểm tra đăng nhập, thành viên phòng, vai trò chủ phòng — `server/handler.c` |
| 6 | Tạo phòng đấu giá | 1 | `CREATE_ROOM` — `server/room.c` |
| 7 | Liệt kê phòng đấu giá | 1 | `LIST_ROOMS` — `server/room.c` |
| 8 | Tạo vật phẩm đấu giá | 3 | `ADD_ITEM` (kiểm tra giá, hàng đợi FIFO, tự động đưa lên sàn, `NOTIFY AUCTION_START`) — `server/item.c`, `server/auction.c` |
| 9 | Xóa vật phẩm trong phòng đấu giá | 1 | `REMOVE_ITEM` (chỉ khi `PENDING`) — `server/item.c` |
| 10 | Tham gia phòng đấu giá | 2 | `JOIN_ROOM`/`LEAVE_ROOM`, giới hạn 1 phòng/người, thông báo thành viên — `server/room.c` |
| 11 | Tố giá | 2 | `BID` (bước giá 10.000đ, broadcast giá mới, đếm ngược 3 phút + 3 lần thông báo) — `server/auction.c` |
| 12 | Mua trực tiếp | 1 | `BUY_NOW` — `server/auction.c` |
| 13 | Ghi log hoạt động | 1 | Mục [7](#7-lưu-trữ-dữ-liệu-và-ghi-log) — `server/logger.c` |
| | **Tổng** | **20** | |

---

## 11. Tài liệu tham khảo

1. W. Richard Stevens, *Unix Network Programming Vol. 1*, 3rd Ed., Prentice Hall.
2. Keir Davis, John W. Turner, Nathan Yocom, *The Definitive Guide to Linux Network Programming*, Apress.
3. Michael Donahoo, Kenneth Calvert, *TCP/IP Sockets in C: Practical Guide for Programmers*, Elsevier.
4. Slide bài giảng môn Lập trình mạng — SoICT, HUST (thư mục [`docs/`](docs/)):
   - Chapter 3 — Socket Introduction
   - Chapter 4 — Basic TCP and UDP Socket
   - Chapter 5 — Multi-Process TCP Server
   - Chapter 6 — Multi-Thread TCP Server
   - Chapter 7 — I/O Multiplexing
