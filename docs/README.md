# Tài liệu dự án — Bidding Platform (TCP Socket, C)

Thư mục `docs/` là **nguồn chân lý duy nhất** cho người và AI coding agent. Slide bài giảng gốc (`docs/Chapter 1–7.pptx`) **không được sửa**; mọi tài liệu thiết kế nằm trong các thư mục con dưới đây.

---

## 1. Bản đồ thư mục

| Thư mục | Vai trò | Ai đọc |
|---|---|---|
| `Chapter 1–7.pptx` | Slide môn Lập trình mạng — **giới hạn kiến thức** được dùng | Mọi agent (qua bản tóm tắt ở `brief/02`) |
| [`brief/`](brief/00-project-brief.md) | **TRƯỚC SPEC** — đề bài, phạm vi, từ vựng, quyết định kiến trúc (ADR), ranh giới kiến thức | Mọi agent, đọc đầu tiên |
| [`specs/`](specs/01-functional-spec.md) | **NGUỒN CHUẨN** — yêu cầu chức năng `REQ-*`, giao thức, thiết kế module/hàm, lưu trữ & log, client | Agent viết code, QA |
| [`plans/`](plans/01-execution-plan.md) | **SAU SPEC** — chia chặng, ranh giới file khi chạy song song, Definition of Done, kịch bản nghiệm thu | Người điều phối, agent thực thi, tester |

Tệp gốc kho: [`README.md`](../README.md) (tổng quan cho người chấm), [`AGENTS.md`](../AGENTS.md) (luật bắt buộc cho agent).

---

## 2. Thứ tự đọc bắt buộc

### A. Agent viết server (`server/`, `common/`)
1. [`brief/00-project-brief.md`](brief/00-project-brief.md) — đề bài, 13 tiêu chí, glossary
2. [`brief/01-architecture-decisions.md`](brief/01-architecture-decisions.md) — ADR-01..10
3. [`brief/02-knowledge-boundary.md`](brief/02-knowledge-boundary.md) — **API nào được dùng**
4. [`specs/01-functional-spec.md`](specs/01-functional-spec.md) — `REQ-*`, ma trận quyền
5. [`specs/02-protocol-spec.md`](specs/02-protocol-spec.md) — cú pháp, mã lỗi, định dạng phản hồi chính xác
6. [`specs/03-server-design-spec.md`](specs/03-server-design-spec.md) — struct, chữ ký hàm, vòng lặp `select()`
7. [`specs/04-storage-and-log-spec.md`](specs/04-storage-and-log-spec.md) — nếu đụng `storage/logger/account/room/item`
8. [`plans/01-execution-plan.md`](plans/01-execution-plan.md) — chặng của mình, file được sửa

### B. Agent viết client (`client/`)
1. `brief/00`, `brief/02`
2. [`specs/02-protocol-spec.md`](specs/02-protocol-spec.md)
3. [`specs/05-client-spec.md`](specs/05-client-spec.md)
4. `plans/01` (Chặng 2C)

### C. QA / Verification agent
1. [`specs/01-functional-spec.md`](specs/01-functional-spec.md), [`specs/02-protocol-spec.md`](specs/02-protocol-spec.md)
2. [`plans/02-acceptance-and-test-plan.md`](plans/02-acceptance-and-test-plan.md)

---

## 3. Quy ước đánh mã

- **Yêu cầu:** `REQ-<NHÓM>-<số>`; nhóm: `STREAM`, `IO`, `ACC`, `SES`, `ACL`, `ROOM`, `ITEM`, `AUC`, `BUY`, `LOG`. Mỗi REQ gắn với **một tiêu chí chấm** (cột `#` ở `brief/00` §3).
- **Quyết định kiến trúc:** `ADR-<số>` trong `brief/01`.
- **Kịch bản nghiệm thu:** `AT-<số>` trong `plans/02`, mỗi AT liệt kê REQ nó phủ.
- **Mã phản hồi giao thức:** chỉ dùng bảng `200/201/400/401/403/404/409/422/500/503` ở `specs/02` §4.

## 4. Quy tắc xung đột & cập nhật

- `specs/` thắng `README.md` gốc khi lệch chi tiết; agent **dừng và báo** người điều phối, không tự chọn.
- Thêm/đổi lệnh, mã, sự kiện, hằng số → sửa **cùng lúc** `specs/02`, `common/protocol.h`, bảng tóm tắt trong `README.md`.
- Dùng API ngoài danh sách `brief/02` → **dừng và hỏi**, không tự thêm.
- Sau khi một chặng merge: tick Definition of Done trong `plans/01` và cột "Trạng thái" trong `plans/02`.
