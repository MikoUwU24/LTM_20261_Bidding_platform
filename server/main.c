#include "common/parse.h"
#include "common/protocol.h"
#include "common/stream.h"
#include "server/account.h"
#include "server/auction.h"
#include "server/handler.h"
#include "server/item.h"
#include "server/logger.h"
#include "server/room.h"
#include "server/session.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void accept_one(int listenfd) {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int cfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
    if (cfd < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return;
        logger_log(LOG_ERROR, "server", "IO accept: %s", strerror(errno));
        return;
    }

    char addr_str[32];
    snprintf(addr_str, sizeof(addr_str), "%s:%u",
             inet_ntoa(cli_addr.sin_addr), (unsigned)ntohs(cli_addr.sin_port));

    if (set_nonblocking(cfd) < 0) {
        logger_log(LOG_ERROR, addr_str, "IO fcntl O_NONBLOCK: %s", strerror(errno));
        close(cfd);
        return;
    }

    session_t *s = session_create(cfd, addr_str);
    if (!s) {
        const char *busy_msg = "503 Server busy\r\n";
        (void)send(cfd, busy_msg, strlen(busy_msg), 0);
        close(cfd);
        logger_log(LOG_WARN, addr_str, "BUSY REJECT(503)");
        return;
    }

    logger_log(LOG_INFO, addr_str, "CONNECT");
}

static void on_readable(session_t *s) {
    char tmp[4096];
    int overflow = 0;

    for (;;) {
        ssize_t n = recv(s->fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            if (stream_recv_append(&s->rbuf, tmp, (size_t)n) < 0) {
                overflow = 1;
                break;
            }
        } else if (n == 0) {
            session_close(s, "peer closed");
            return;
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            logger_log(LOG_ERROR, session_subject(s), "IO recv: %s", strerror(errno));
            session_close(s, strerror(errno));
            return;
        }
    }

    if (overflow) {
        session_send(s, "400 Line too long");
        logger_log(LOG_WARN, session_subject(s), "LINE_TOO_LONG REJECT(400)");
        s->closing = 1;
        return;
    }

    char line[MAX_LINE + 1];
    while (s->in_use && !s->closing) {
        int rc = stream_next_line(&s->rbuf, line, sizeof(line), MAX_LINE);
        if (rc == 1) {
            if (line[0] == '\0') continue;
            handler_process_line(s, line);
        } else if (rc == 0) {
            break;
        } else {
            session_send(s, "400 Line too long");
            logger_log(LOG_WARN, session_subject(s), "LINE_TOO_LONG REJECT(400)");
            s->closing = 1;
            break;
        }
    }
}

static void on_writable(session_t *s) {
    if (stream_flush(s->fd, &s->sbuf) < 0) {
        logger_log(LOG_ERROR, session_subject(s), "IO send: %s", strerror(errno));
        session_close(s, "send error");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    long long port_ll = 0;
    if (parse_ll(argv[1], 1, 65535, &port_ll) < 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    if (access(DATA_DIR, R_OK | W_OK | X_OK) != 0 ||
        access("logs", R_OK | W_OK | X_OK) != 0) {
        fprintf(stderr, "Missing data/ or logs/ directory. Run: mkdir -p data logs\n");
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));
    signal(SIGPIPE, SIG_IGN);

    logger_init(LOG_FILE);
    session_init_all();

    account_load();
    room_load();
    item_load();
    auction_resume_all(time(NULL));

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listenfd);
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((uint16_t)port_ll);

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listenfd);
        return 1;
    }

    if (listen(listenfd, BACKLOG) < 0) {
        perror("listen");
        close(listenfd);
        return 1;
    }

    logger_log(LOG_INFO, "server", "START port=%lld", port_ll);

    for (;;) {
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(listenfd, &rfds);
        int maxfd = listenfd;

        for (int i = 0; i < session_capacity(); i++) {
            session_t *s = session_at(i);
            if (!s) continue;
            FD_SET(s->fd, &rfds);
            if (stream_send_pending(&s->sbuf)) {
                FD_SET(s->fd, &wfds);
            }
            if (s->fd > maxfd) maxfd = s->fd;
        }

        struct timeval tv;
        struct timeval *tvp = NULL;
        long next_to = auction_next_timeout(time(NULL));
        if (next_to >= 0) {
            tv.tv_sec = next_to;
            tv.tv_usec = 0;
            tvp = &tv;
        }

        int nready = select(maxfd + 1, &rfds, &wfds, NULL, tvp);
        if (nready < 0) {
            if (errno == EINTR) continue;
            logger_log(LOG_ERROR, "server", "IO select: %s", strerror(errno));
            continue;
        }

        if (FD_ISSET(listenfd, &rfds)) {
            accept_one(listenfd);
        }

        for (int i = 0; i < session_capacity(); i++) {
            session_t *s = session_at(i);
            if (!s) continue;
            if (FD_ISSET(s->fd, &rfds)) {
                on_readable(s);
            }
            s = session_at(i);
            if (s && FD_ISSET(s->fd, &wfds)) {
                on_writable(s);
            }
            s = session_at(i);
            if (s && s->closing && !stream_send_pending(&s->sbuf)) {
                session_close(s, "closing");
            }
        }

        auction_tick(time(NULL));
    }

    close(listenfd);
    logger_close();
    return 0;
}
