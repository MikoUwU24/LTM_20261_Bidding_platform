#include "common/stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void test_partial_and_sticky_lines(void) {
    recv_buffer_t rb;
    stream_recv_init(&rb);
    char out[256];

    /* Nhận thiếu */
    assert(stream_recv_append(&rb, "LOGIN ali", 9) == 0);
    assert(stream_next_line(&rb, out, sizeof(out), MAX_LINE) == 0);
    assert(stream_recv_append(&rb, "ce 1234\r\n", 9) == 0);
    assert(stream_next_line(&rb, out, sizeof(out), MAX_LINE) == 1);
    assert(strcmp(out, "LOGIN alice 1234") == 0);

    /* Dính 3 dòng + dung thứ '\n' không có '\r' */
    const char *sticky = "CMD1 a\r\nCMD2 b\nCMD3 c\r\n";
    assert(stream_recv_append(&rb, sticky, strlen(sticky)) == 0);
    assert(stream_next_line(&rb, out, sizeof(out), MAX_LINE) == 1);
    assert(strcmp(out, "CMD1 a") == 0);
    assert(stream_next_line(&rb, out, sizeof(out), MAX_LINE) == 1);
    assert(strcmp(out, "CMD2 b") == 0);
    assert(stream_next_line(&rb, out, sizeof(out), MAX_LINE) == 1);
    assert(strcmp(out, "CMD3 c") == 0);
    assert(stream_next_line(&rb, out, sizeof(out), MAX_LINE) == 0);
}

static void test_max_line_boundary(void) {
    recv_buffer_t rb;
    stream_recv_init(&rb);
    char out[32];

    /* Đúng bằng max_line = 10 ("12345678\r\n") → hợp lệ */
    assert(stream_recv_append(&rb, "12345678\r\n", 10) == 0);
    assert(stream_next_line(&rb, out, sizeof(out), 10) == 1);
    assert(strcmp(out, "12345678") == 0);

    /* Dài hơn max_line = 10 ("123456789\r\n" = 11) → -1 */
    stream_recv_init(&rb);
    assert(stream_recv_append(&rb, "123456789\r\n", 11) == 0);
    assert(stream_next_line(&rb, out, sizeof(out), 10) == -1);

    /* Chưa có '\n' nhưng đã đủ 10 byte → -1 */
    stream_recv_init(&rb);
    assert(stream_recv_append(&rb, "0123456789", 10) == 0);
    assert(stream_next_line(&rb, out, sizeof(out), 10) == -1);
}

static void test_send_flush(void) {
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    send_buffer_t sb;
    stream_send_init(&sb);
    assert(stream_send_pending(&sb) == 0);

    const char *msg = "200 OK\r\nNOTIFY NEW_BID 1 bob 500000\r\n";
    assert(stream_send_append(&sb, msg, strlen(msg)) == 0);
    assert(stream_send_pending(&sb) == 1);

    ssize_t flushed = stream_flush(sv[0], &sb);
    assert(flushed == (ssize_t)strlen(msg));
    assert(stream_send_pending(&sb) == 0);

    char rbuf[128] = {0};
    ssize_t r = recv(sv[1], rbuf, sizeof(rbuf) - 1, 0);
    assert(r == (ssize_t)strlen(msg));
    assert(strcmp(rbuf, msg) == 0);

    close(sv[0]);
    close(sv[1]);
}

int main(void) {
    test_partial_and_sticky_lines();
    test_max_line_boundary();
    test_send_flush();
    printf("PASS unit/test_stream\n");
    return 0;
}
