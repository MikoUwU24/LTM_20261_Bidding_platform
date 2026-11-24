#include "client/ui.h"
#include "common/parse.h"
#include "common/protocol.h"
#include "common/stream.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip|hostname> <port>\n", argv[0]);
        return 1;
    }

    long long port_ll = 0;
    if (parse_ll(argv[2], 1, 65535, &port_ll) < 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)port_ll);

    if (inet_aton(argv[1], &serv_addr.sin_addr) == 0) {
        struct hostent *he = gethostbyname(argv[1]);
        if (!he || !he->h_addr_list[0]) {
            fprintf(stderr, "Cannot resolve host: %s\n", argv[1]);
            return 1;
        }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    }

    signal(SIGPIPE, SIG_IGN);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Connected to %s:%lld. Type 'help' for commands.\n", argv[1], port_ll);
    ui_prompt();

    recv_buffer_t rbuf;
    stream_recv_init(&rbuf);

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sock, &rfds);
        int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        int nready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(sock, &rfds)) {
            char tmp[4096];
            ssize_t n = recv(sock, tmp, sizeof(tmp), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("recv");
                break;
            }
            if (n == 0) {
                printf("\rServer closed connection\n");
                break;
            }
            if (stream_recv_append(&rbuf, tmp, (size_t)n) < 0) {
                fprintf(stderr, "\rReceive buffer overflow\n");
                break;
            }
            char line[MAX_RESP_LINE];
            for (;;) {
                int rc = stream_next_line(&rbuf, line, sizeof(line), MAX_RESP_LINE);
                if (rc == 1) {
                    ui_print_server_line(line);
                } else {
                    break;
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char input[MAX_LINE];
            if (fgets(input, sizeof(input), stdin) == NULL) {
                break;
            }
            int action = ui_translate_input(input);
            if (action < 0) {
                break;
            }
            if (action == 1) {
                char out[MAX_LINE + 4];
                int len = snprintf(out, sizeof(out), "%s\r\n", input);
                if (len > 0 && stream_send_all(sock, out, (size_t)len) < 0) {
                    perror("send");
                    break;
                }
            } else {
                ui_prompt();
            }
        }
    }

    close(sock);
    return 0;
}
