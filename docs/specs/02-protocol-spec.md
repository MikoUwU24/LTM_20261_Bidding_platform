# 02 — Protocol Specification

> **Trạng thái:** Nguồn chuẩn. Mọi chuỗi trong tài liệu này là hợp đồng: client và test so khớp theo nó.
> Mọi tên lệnh, mã, sự kiện, hằng số phải có trong `common/protocol.h`.

---

## 1. Đóng khung & mã hoá

| Mục | Quy định |
|---|---|
| Kết thúc dòng | Gửi: luôn `\r\n`. Nhận (server): `\n`, bỏ một `\r` đứng ngay trước nếu có |
| Mã ký tự | ASCII/UTF-8 byte; byte `< 0x20` (trừ kết thúc dòng) trong request → `400` |
| Dòng request tối đa | `MAX_LINE` = 4096 byte kể cả kết thúc dòng → vượt: `400 Line too long` + đóng |
| Dòng response tối đa | `MAX_RESP_LINE` = 16384 byte (client phải có bộ đệm ≥ giá trị này) |
| Dòng rỗng | Bỏ qua, không phản hồi |
| Tên lệnh | Chữ HOA, phân biệt hoa thường (`login` → `400 Unknown command`) |

## 2. Tách tham số (`parse_tokenize`)

1. Bỏ qua khoảng trắng (dấu cách) đầu dòng và giữa các token; nhiều dấu cách liên tiếp = một.
2. Token bắt đầu bằng `"` kéo dài tới `"` kế tiếp; nội dung **không** chứa `"`; được phép rỗng (`""`). Sau `"` đóng phải là dấu cách hoặc hết dòng, nếu không → `400`.
3. Thiếu `"` đóng → `400 Unterminated quote`.
4. Tối đa `MAX_TOKENS` = 8 token; vượt → `400`.
5. Token đã tách không còn dấu `"`. Kiểm tra "có phải chuỗi ngoặc kép" chỉ quan trọng khi **server gửi** (luôn bọc tên/mô tả trong `"`).

## 3. Lệnh

Cột "Guard" dùng cờ ở spec 03 §4. `argc` không tính tên lệnh.

| Lệnh | Cú pháp | argc | Guard | Phản hồi thành công |
|---|---|:-:|---|---|
| Đăng ký | `REGISTER <username> <password>` | 2 | `G_NOT_AUTH` | `201 Registered <username>` |
| Đăng nhập | `LOGIN <username> <password>` | 2 | `G_NOT_AUTH` | `200 Login successful` |
| Đăng xuất | `LOGOUT` | 0 | `G_AUTH` | `200 Logged out` |
| Tạo phòng | `CREATE_ROOM "<name>"` | 1 | `G_AUTH\|G_NOT_IN_ROOM` | `201 Room created <room_id>` |
| Liệt kê phòng | `LIST_ROOMS` | 0 | `G_AUTH` | `200 ROOMS <n>` + bản ghi (§5) |
| Vào phòng | `JOIN_ROOM <room_id>` | 1 | `G_AUTH\|G_NOT_IN_ROOM` | `200 Joined room <room_id> "<name>"` |
| Rời phòng | `LEAVE_ROOM` | 0 | `G_AUTH\|G_IN_ROOM` | `200 Left room <room_id>` |
| Xem vật phẩm | `LIST_ITEMS` | 0 | `G_AUTH\|G_IN_ROOM` | `200 ITEMS <n>` + bản ghi (§5) |
| Đăng bán | `ADD_ITEM "<name>" "<desc>" <start_price> <buy_now_price>` | 4 | `G_AUTH\|G_IN_ROOM\|G_OWNER` | `201 Item added <item_id>` |
| Rút vật phẩm | `REMOVE_ITEM <item_id>` | 1 | `G_AUTH\|G_IN_ROOM\|G_OWNER` | `200 Item removed <item_id>` |
| Tố giá | `BID <item_id> <price>` | 2 | `G_AUTH\|G_IN_ROOM\|G_NOT_OWNER` | `200 Bid accepted` — hoặc nếu `price >= buy_now_price`: `200 Bought item <item_id> price <buy_now_price>` |
| Mua ngay | `BUY_NOW <item_id>` | 1 | `G_AUTH\|G_IN_ROOM\|G_NOT_OWNER` | `200 Bought item <item_id> price <buy_now_price>` |

**Số nguyên** (`room_id`, `item_id`, giá): chỉ chữ số `0-9`, không dấu, không khoảng trắng; parse bằng `strtoll` + kiểm `ERANGE`; id `≥ 1`; giá `1..MAX_PRICE`. Sai → `400`.

## 4. Mã phản hồi & thông điệp lỗi chuẩn

| Mã | Ý nghĩa | Thông điệp dùng (giữ nguyên chữ) |
|:-:|---|---|
| `200` | Thành công | xem §3 |
| `201` | Tạo mới thành công | xem §3 |
| `400` | Sai cú pháp/tham số | `Unknown command` · `Wrong number of arguments` · `Invalid username` · `Invalid password` · `Invalid room name` · `Invalid item name` · `Invalid description` · `Invalid number` · `Unterminated quote` · `Line too long` |
| `401` | Chưa đăng nhập / sai thông tin | `Not logged in` · `Invalid username or password` |
| `403` | Không có quyền | `Not in a room` · `Only room owner can do this` · `Room owner cannot bid` |
| `404` | Không tìm thấy | `Room not found` · `Item not found` |
| `409` | Xung đột trạng thái | `Already logged in` · `Account is logged in elsewhere` · `Username already exists` · `Already in a room` · `Item is not pending` · `Item is not on auction` · `Item queue is full` |
| `422` | Giá không hợp lệ | `Bid must be at least <min>` · `Buy-now price must be greater than start price` |
| `500` | Lỗi server | `Internal error` · `Server storage full` |
| `503` | Quá tải | `Server busy` |

## 5. Định dạng danh sách (một dòng duy nhất)

Bản ghi ngăn cách bằng `; ` (dấu chấm phẩy + dấu cách); trường trong bản ghi ngăn bằng dấu cách; chuỗi luôn trong `"`. Client tách bằng cùng `parse_tokenize` sau khi cắt theo `; ` **bên ngoài** ngoặc kép.

```
200 ROOMS <n>[; <room_id> <owner> "<name>" <members> <on_auction_item_id|-> <pending_count>]...
200 ITEMS <n>[; <item_id> <STATUS> "<name>" "<desc>" <start_price> <buy_now_price> <current_price> <winner|->]...
```

Ví dụ:
```
200 ROOMS 2; 1 alice "Do co" 3 12 2; 2 bob "Tranh son dau" 1 - 0
200 ITEMS 2; 12 ON_AUCTION "Binh gom" "Gom Bat Trang" 500000 2000000 600000 carol; 13 PENDING "Lo hoa" "" 100000 300000 100000 -
```

## 6. Thông báo bất đồng bộ `NOTIFY`

| Sự kiện | Cú pháp | Người nhận |
|---|---|---|
| Vào phòng | `NOTIFY USER_JOINED <username>` | Mọi người trong phòng **trừ** người vào |
| Rời phòng | `NOTIFY USER_LEFT <username>` | Mọi người còn lại trong phòng |
| Thêm vật phẩm | `NOTIFY ITEM_ADDED <item_id> "<name>"` | Cả phòng |
| Rút vật phẩm | `NOTIFY ITEM_REMOVED <item_id>` | Cả phòng |
| Lên sàn | `NOTIFY AUCTION_START <item_id> "<name>" "<desc>" <start_price> <buy_now_price>` | Cả phòng; riêng người vừa `JOIN_ROOM` |
| Giá mới | `NOTIFY NEW_BID <item_id> <username> <price>` | Cả phòng; riêng người vừa `JOIN_ROOM` nếu đã có giá |
| Đếm ngược | `NOTIFY COUNTDOWN <item_id> <1\|2\|3> <current_price>` | Cả phòng |
| Đã bán | `NOTIFY SOLD <item_id> <username> <price>` | Cả phòng |
| Không bán được | `NOTIFY UNSOLD <item_id>` | Cả phòng |

**Thứ tự bắt buộc (ADR-04):** phản hồi của yêu cầu luôn đứng trước mọi `NOTIFY` do chính yêu cầu đó sinh ra gửi tới cùng client. Chuỗi sự kiện trong một lần xử lý:

| Tình huống | Thứ tự dòng mà người thực hiện nhận |
|---|---|
| `ADD_ITEM` khi phòng rảnh | `201 Item added 13` → `NOTIFY ITEM_ADDED 13 "..."` → `NOTIFY AUCTION_START 13 ...` |
| `BID` hợp lệ | `200 Bid accepted` → `NOTIFY NEW_BID ...` |
| `BUY_NOW` | `200 Bought item ...` → `NOTIFY SOLD ...` → (`NOTIFY AUCTION_START` vật phẩm kế tiếp nếu có) |
| `JOIN_ROOM` | `200 Joined room ...` → (`NOTIFY AUCTION_START ...` → `NOTIFY NEW_BID ...`) |

## 7. Phiên mẫu

```
C: REGISTER alice 123456
S: 201 Registered alice
C: LOGIN alice 123456
S: 200 Login successful
C: CREATE_ROOM "Do co"
S: 201 Room created 1
C: ADD_ITEM "Binh gom" "Gom Bat Trang the ky 19" 500000 2000000
S: 201 Item added 1
S: NOTIFY ITEM_ADDED 1 "Binh gom"
S: NOTIFY AUCTION_START 1 "Binh gom" "Gom Bat Trang the ky 19" 500000 2000000
                     --- client bob (đã LOGIN) ---
C: JOIN_ROOM 1
S: 200 Joined room 1 "Do co"
S: NOTIFY AUCTION_START 1 "Binh gom" "Gom Bat Trang the ky 19" 500000 2000000
C: BID 1 505000
S: 422 Bid must be at least 510000
C: BID 1 600000
S: 200 Bid accepted
S: NOTIFY NEW_BID 1 bob 600000
   ... 180 giây không có giá mới ...
S: NOTIFY COUNTDOWN 1 1 600000
S: NOTIFY COUNTDOWN 1 2 600000
S: NOTIFY COUNTDOWN 1 3 600000
S: NOTIFY SOLD 1 bob 600000
```
