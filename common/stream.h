#ifndef COMMON_STREAM_H
#define COMMON_STREAM_H

#include <stddef.h>
#include <sys/types.h>
#include "common/protocol.h"

typedef struct {
    char   buf[RECV_BUF_SIZE];
    size_t len;
} recv_buffer_t;

typedef struct {
    char   buf[SEND_BUF_SIZE];
    size_t len;
    size_t off;
} send_buffer_t;

void    stream_recv_init(recv_buffer_t *rb);
/* Nối n byte vào cuối bộ đệm. 0 = OK, -1 = không đủ chỗ. */
int     stream_recv_append(recv_buffer_t *rb, const char *data, size_t n);
/* Lấy một dòng (bỏ \n và \r cuối) vào out. 1 = có dòng, 0 = chưa đủ,
   -1 = dòng dài hơn max_line (gồm kết thúc dòng) hoặc không vừa out. */
int     stream_next_line(recv_buffer_t *rb, char *out, size_t cap, size_t max_line);

void    stream_send_init(send_buffer_t *sb);
int     stream_send_append(send_buffer_t *sb, const char *data, size_t n);
int     stream_send_pending(const send_buffer_t *sb);
/* Gửi phần còn lại bằng send(); xử lý EINTR; EAGAIN → trả 0.
   Trả số byte đã gửi (≥0) hoặc -1 nếu lỗi thật. Dồn bộ đệm khi off > len/2. */
ssize_t stream_flush(int fd, send_buffer_t *sb);
/* Client (socket chặn): gửi hết n byte, lặp khi gửi thiếu/EINTR. 0 = OK, -1 = lỗi. */
int     stream_send_all(int fd, const char *data, size_t n);

#endif /* COMMON_STREAM_H */
