#include "common/stream.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

void stream_recv_init(recv_buffer_t *rb) {
    if (!rb) return;
    rb->len = 0;
}

int stream_recv_append(recv_buffer_t *rb, const char *data, size_t n) {
    if (!rb || (!data && n > 0)) return -1;
    if (n == 0) return 0;
    if (rb->len + n > RECV_BUF_SIZE) return -1;
    memcpy(rb->buf + rb->len, data, n);
    rb->len += n;
    return 0;
}

int stream_next_line(recv_buffer_t *rb, char *out, size_t cap, size_t max_line) {
    if (!rb || !out || cap == 0) return -1;

    char *nl = memchr(rb->buf, '\n', rb->len);
    if (!nl) {
        /* Chưa có '\n' nhưng số byte hiện tại đã đạt/vượt max_line → quá dài */
        if (rb->len >= max_line) return -1;
        return 0;
    }

    size_t raw_len = (size_t)(nl - rb->buf) + 1; /* tính cả '\n' */
    if (raw_len > max_line) return -1;

    size_t content_len = raw_len - 1;
    if (content_len > 0 && rb->buf[content_len - 1] == '\r') {
        content_len--;
    }
    if (content_len + 1 > cap) return -1;

    memcpy(out, rb->buf, content_len);
    out[content_len] = '\0';

    size_t remain = rb->len - raw_len;
    if (remain > 0) {
        memmove(rb->buf, rb->buf + raw_len, remain);
    }
    rb->len = remain;
    return 1;
}

void stream_send_init(send_buffer_t *sb) {
    if (!sb) return;
    sb->len = 0;
    sb->off = 0;
}

int stream_send_append(send_buffer_t *sb, const char *data, size_t n) {
    if (!sb || (!data && n > 0)) return -1;
    if (n == 0) return 0;

    /* Dồn dữ liệu về đầu nếu cần chỗ */
    if (sb->len + n > SEND_BUF_SIZE && sb->off > 0) {
        size_t pending = sb->len - sb->off;
        if (pending > 0) {
            memmove(sb->buf, sb->buf + sb->off, pending);
        }
        sb->len = pending;
        sb->off = 0;
    }
    if (sb->len + n > SEND_BUF_SIZE) return -1;

    memcpy(sb->buf + sb->len, data, n);
    sb->len += n;
    return 0;
}

int stream_send_pending(const send_buffer_t *sb) {
    if (!sb) return 0;
    return sb->len > sb->off ? 1 : 0;
}

ssize_t stream_flush(int fd, send_buffer_t *sb) {
    if (!sb) return -1;
    if (sb->off >= sb->len) {
        sb->len = 0;
        sb->off = 0;
        return 0;
    }

    size_t to_send = sb->len - sb->off;
    ssize_t sent;
    do {
        sent = send(fd, sb->buf + sb->off, to_send, 0);
    } while (sent < 0 && errno == EINTR);

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }

    sb->off += (size_t)sent;
    if (sb->off == sb->len) {
        sb->len = 0;
        sb->off = 0;
    } else if (sb->off > sb->len / 2) {
        size_t remain = sb->len - sb->off;
        memmove(sb->buf, sb->buf + sb->off, remain);
        sb->len = remain;
        sb->off = 0;
    }
    return sent;
}

int stream_send_all(int fd, const char *data, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t sent = send(fd, data + total, n - total, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (sent == 0) return -1;
        total += (size_t)sent;
    }
    return 0;
}
