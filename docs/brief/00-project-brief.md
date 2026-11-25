# 00 — Project Brief, Phạm vi & Glossary

> **Trạng thái:** Đã chốt (2026-11-25)
> **Nguồn yêu cầu:** Đề bài số 9 "Xây dựng ứng dụng đấu giá trực tuyến" — môn Lập trình mạng, SoICT – HUST ([ảnh đề bài](assets/de-bai.png)).

---

## 1. Đề bài (chép nguyên văn)

> Mỗi người tham gia đấu giá có thể thực hiện các chức năng sau:
> - Tạo phòng đấu giá
> - Tham gia một phòng đấu giá. Tại một thời điểm, mỗi người chỉ được tham gia vào 1 phòng đấu giá.
> - Bán vật phẩm trong phòng đấu giá do người dùng tạo. Mỗi vật phẩm bán đấu giá cần có thông tin về vật phẩm, giá khởi điểm, giá bán ngay. Người dùng có thể rút vật phẩm ra khỏi phòng đấu giá nếu vật phẩm đó chưa được đem ra đấu giá. Các vật phẩm được đăng bán lần lượt sẽ được giới thiệu trong phiên bán.
> - Đấu giá vật phẩm. Người dùng phải trả giá lớn hơn giá hiện tại ít nhất 10.000 đồng. Nếu lệnh đặt giá hợp lệ, những người dùng trong phòng đấu giá được thông báo giá mới của vật phẩm. Sau mỗi lần đặt giá 3 phút, nếu không có ai trả giá cao hơn, server gửi thông báo 3 lần, mỗi lần cách nh*[đề bị cắt]*
> - Mua ngay vật phẩm không qua đấu giá

**Giả định cho phần bị cắt:** mỗi lần thông báo cách nhau **10 giây** (`COUNTDOWN_INTERVAL_SEC`), sau lần thứ 3 vật phẩm bán cho người trả giá cao nhất (xem ADR-03).

---

## 2. Mục tiêu

1. Server TCP một tiến trình phục vụ nhiều client đồng thời, giao thức văn bản tự định nghĩa.
2. Đạt đủ **20/20 điểm** theo 13 tiêu chí ở §3; mỗi tiêu chí có REQ, có code, có kịch bản test.
3. **Chỉ dùng kiến thức trong slide** `docs/Chapter 1–7.pptx` (chi tiết: [`02-knowledge-boundary.md`](02-knowledge-boundary.md)).

---

## 3. Tiêu chí chấm → REQ → nơi cài đặt

| # | Tiêu chí | Điểm | REQ | File chính |
|---|---|:-:|---|---|
| 1 | Xử lý truyền dòng | 1 | `REQ-STREAM-001..004` | `common/stream.c` |
| 2 | Cài đặt cơ chế vào/ra socket trên server | 2 | `REQ-IO-001..006` | `server/main.c`, `server/session.c` |
| 3 | Đăng ký và quản lý tài khoản | 2 | `REQ-ACC-001..004` | `server/account.c`, `server/storage.c` |
| 4 | Đăng nhập và quản lý phiên | 2 | `REQ-SES-001..005` | `server/handler.c`, `server/session.c` |
| 5 | Kiểm soát quyền truy cập phòng đấu giá | 1 | `REQ-ACL-001..003` | `server/handler.c` |
| 6 | Tạo phòng đấu giá | 1 | `REQ-ROOM-001` | `server/room.c` |
| 7 | Liệt kê phòng đấu giá | 1 | `REQ-ROOM-002` | `server/room.c` |
| 8 | Tạo vật phẩm đấu giá | 3 | `REQ-ITEM-001..004` | `server/item.c`, `server/auction.c` |
| 9 | Xóa vật phẩm trong phòng đấu giá | 1 | `REQ-ITEM-005` | `server/item.c` |
| 10 | Tham gia phòng đấu giá | 2 | `REQ-ROOM-003..005` | `server/room.c` |
| 11 | Tố giá | 2 | `REQ-AUC-001..006` | `server/auction.c` |
| 12 | Mua trực tiếp | 1 | `REQ-BUY-001..003` | `server/auction.c` |
| 13 | Ghi log hoạt động | 1 | `REQ-LOG-001..003` | `server/logger.c` |
| | **Tổng** | **20** | | |

---

## 4. Phạm vi

### Trong phạm vi
- Server C11 trên Linux/WSL, `select()`, giao thức dòng văn bản `\r\n`.
- Client CLI (`select()` trên `stdin` + socket).
- Lưu file văn bản `data/*.txt`, log `logs/server.log`.
- Script kiểm thử bằng `bash` + `nc` (`tests/`), unit test C thuần (`tests/unit/`).

### Ngoài phạm vi (không code)
- GUI, web, mã hoá TLS, IPv6, UDP, đa luồng, đa tiến trình.
- Xoá phòng, đổi mật khẩu, nạp/thanh toán tiền, lịch sử giao dịch chi tiết, chat trong phòng.
- Cơ sở dữ liệu, thư viện ngoài (OpenSSL, libevent, JSON…).

---

## 5. Glossary (dùng thống nhất trong code, log, tài liệu)

| Thuật ngữ | Tên trong code | Nghĩa |
|---|---|---|
| Phiên | `session_t` | Trạng thái của một kết nối TCP (fd, bộ đệm, user, phòng) |
| Tài khoản | `account_t` | `username` + `salt` + `hash` mật khẩu |
| Phòng | `room_t` | Phòng đấu giá; có `owner`, hàng đợi vật phẩm, bộ đếm |
| Chủ phòng | owner | Người tạo phòng (`room.owner == session.username`) |
| Thành viên | member | Người đang ở trong phòng, không phải owner |
| Vật phẩm | `item_t` | Hàng được đăng bán trong một phòng |
| Trạng thái vật phẩm | `item_status_t` | `PENDING`, `ON_AUCTION`, `SOLD`, `UNSOLD`, `REMOVED` |
| Giá hiện tại | `current_price` | Giá cao nhất đã đặt; bằng `start_price` khi chưa ai đặt |
| Người thắng tạm | `winner` | Người đặt `current_price`; rỗng (`-` trong file) khi chưa ai đặt |
| Bước giá | `BID_STEP` | 10.000 đồng |
| Bộ đếm | `deadline`, `countdown_stage` | Thời điểm hết hạn kế tiếp và số lần `COUNTDOWN` đã gửi (0..3) |
| Thông báo | `NOTIFY ...` | Dòng server tự gửi, không phải phản hồi của yêu cầu |
| Người điều phối | coordinator | Người/agent duy nhất được sửa file dùng chung (xem `plans/01`) |
