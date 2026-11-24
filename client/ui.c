#include "client/ui.h"
#include "common/parse.h"
#include "common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void format_price(long long price, char *out, size_t cap) {
    char raw[32];
    int n = snprintf(raw, sizeof(raw), "%lld", price);
    if (n <= 0) {
        snprintf(out, cap, "0");
        return;
    }
    size_t pos = 0;
    for (int i = 0; i < n && pos + 2 < cap; i++) {
        if (i > 0 && (n - i) % 3 == 0) {
            out[pos++] = '.';
        }
        out[pos++] = raw[i];
    }
    out[pos] = '\0';
}

void ui_prompt(void) {
    printf("> ");
    fflush(stdout);
}

void ui_print_help(void) {
    printf("Commands:\n"
           "  REGISTER <username> <password>\n"
           "  LOGIN <username> <password>\n"
           "  LOGOUT\n"
           "  CREATE_ROOM \"<room_name>\"\n"
           "  LIST_ROOMS\n"
           "  JOIN_ROOM <room_id>\n"
           "  LEAVE_ROOM\n"
           "  LIST_ITEMS\n"
           "  ADD_ITEM \"<name>\" \"<desc>\" <start_price> <buy_now_price>\n"
           "  REMOVE_ITEM <item_id>\n"
           "  BID <item_id> <price>\n"
           "  BUY_NOW <item_id>\n"
           "  help | quit\n");
}

static void print_rooms_list(char *rest) {
    char *records[MAX_ROOMS + 2];
    int nrec = parse_split_records(rest, records, MAX_ROOMS + 2);
    if (nrec <= 0) return;

    printf("\r[OK] %s\n", records[0]);
    if (nrec > 1) {
        printf("  %-4s %-16s %-24s %-7s %-8s %-7s\n",
               "ID", "OWNER", "NAME", "MEMBERS", "AUCTION", "PENDING");
        for (int i = 1; i < nrec; i++) {
            char *tok[8];
            int nt = parse_tokenize(records[i], tok, 8);
            if (nt == 6) {
                printf("  %-4s %-16s %-24s %-7s %-8s %-7s\n",
                       tok[0], tok[1], tok[2], tok[3], tok[4], tok[5]);
            }
        }
    }
}

static void print_items_list(char *rest) {
    char *records[MAX_ITEMS + 2];
    int nrec = parse_split_records(rest, records, MAX_ITEMS + 2);
    if (nrec <= 0) return;

    printf("\r[OK] %s\n", records[0]);
    if (nrec > 1) {
        printf("  %-4s %-11s %-18s %-14s %-14s %-14s %-12s\n",
               "ID", "STATUS", "NAME", "START", "BUY_NOW", "CURRENT", "WINNER");
        for (int i = 1; i < nrec; i++) {
            char *tok[10];
            int nt = parse_tokenize(records[i], tok, 10);
            if (nt == 8) {
                char s_str[32], b_str[32], c_str[32];
                format_price(atoll(tok[4]), s_str, sizeof(s_str));
                format_price(atoll(tok[5]), b_str, sizeof(b_str));
                format_price(atoll(tok[6]), c_str, sizeof(c_str));
                printf("  %-4s %-11s %-18s %-14s %-14s %-14s %-12s\n",
                       tok[0], tok[1], tok[2], s_str, b_str, c_str, tok[7]);
            }
        }
    }
}

static void print_notify(char *body) {
    char copy[MAX_RESP_LINE];
    snprintf(copy, sizeof(copy), "%s", body);
    char *tok[MAX_TOKENS];
    int nt = parse_tokenize(copy, tok, MAX_TOKENS);
    if (nt <= 0) {
        printf("\r>> %s\n", body);
        return;
    }

    if (strcmp(tok[0], EVT_AUCTION_START) == 0 && nt == 6) {
        char s_str[32], b_str[32];
        format_price(atoll(tok[4]), s_str, sizeof(s_str));
        format_price(atoll(tok[5]), b_str, sizeof(b_str));
        printf("\r>> Đấu giá: #%s %s — %s | khởi điểm %s | mua ngay %s\n",
               tok[1], tok[2], tok[3], s_str, b_str);
    } else if (strcmp(tok[0], EVT_NEW_BID) == 0 && nt == 4) {
        char p_str[32];
        format_price(atoll(tok[3]), p_str, sizeof(p_str));
        printf("\r>> Giá mới #%s: %s bởi %s\n", tok[1], p_str, tok[2]);
    } else if (strcmp(tok[0], EVT_COUNTDOWN) == 0 && nt == 4) {
        char p_str[32];
        format_price(atoll(tok[3]), p_str, sizeof(p_str));
        printf("\r>> Đếm ngược lần %s/3 #%s — giá %s\n", tok[2], tok[1], p_str);
    } else if (strcmp(tok[0], EVT_SOLD) == 0 && nt == 4) {
        char p_str[32];
        format_price(atoll(tok[3]), p_str, sizeof(p_str));
        printf("\r>> ĐÃ BÁN #%s cho %s giá %s\n", tok[1], tok[2], p_str);
    } else if (strcmp(tok[0], EVT_UNSOLD) == 0 && nt == 2) {
        printf("\r>> KHÔNG BÁN ĐƯỢC #%s\n", tok[1]);
    } else {
        printf("\r>> %s\n", body);
    }
}

void ui_print_server_line(const char *line) {
    if (!line) return;
    char buf[MAX_RESP_LINE];
    snprintf(buf, sizeof(buf), "%s", line);

    if (strncmp(buf, "NOTIFY ", 7) == 0) {
        print_notify(buf + 7);
        ui_prompt();
        return;
    }

    if (strlen(buf) >= 4 && buf[3] == ' ') {
        char code_str[4] = { buf[0], buf[1], buf[2], '\0' };
        int code = atoi(code_str);
        char *msg = buf + 4;
        if (code >= 200 && code < 300) {
            if (strncmp(msg, "ROOMS ", 6) == 0) {
                print_rooms_list(msg);
            } else if (strncmp(msg, "ITEMS ", 6) == 0) {
                print_items_list(msg);
            } else {
                printf("\r[OK] %s\n", msg);
            }
            ui_prompt();
            return;
        } else if (code >= 400 && code < 600) {
            printf("\r[ERR %d] %s\n", code, msg);
            ui_prompt();
            return;
        }
    }

    printf("\r%s\n", buf);
    ui_prompt();
}

int ui_translate_input(char *line) {
    if (!line) return 0;
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
    while (*line == ' ') {
        memmove(line, line + 1, strlen(line));
    }
    if (line[0] == '\0') return 0;
    if (strcmp(line, "help") == 0) {
        ui_print_help();
        return 0;
    }
    if (strcmp(line, "quit") == 0) {
        return -1;
    }
    return 1;
}
