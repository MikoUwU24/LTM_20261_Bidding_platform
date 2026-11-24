#include "server/handler.h"
#include "common/parse.h"
#include "common/protocol.h"
#include "server/account.h"
#include "server/auction.h"
#include "server/item.h"
#include "server/logger.h"
#include "server/room.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const command_t g_commands[] = {
    { CMD_REGISTER,    2, G_NOT_AUTH,                         cmd_register    },
    { CMD_LOGIN,       2, G_NOT_AUTH,                         cmd_login       },
    { CMD_LOGOUT,      0, G_AUTH,                             cmd_logout      },
    { CMD_CREATE_ROOM, 1, G_AUTH | G_NOT_IN_ROOM,             cmd_create_room },
    { CMD_LIST_ROOMS,  0, G_AUTH,                             cmd_list_rooms  },
    { CMD_JOIN_ROOM,   1, G_AUTH | G_NOT_IN_ROOM,             cmd_join_room   },
    { CMD_LEAVE_ROOM,  0, G_AUTH | G_IN_ROOM,                 cmd_leave_room  },
    { CMD_LIST_ITEMS,  0, G_AUTH | G_IN_ROOM,                 cmd_list_items  },
    { CMD_ADD_ITEM,    4, G_AUTH | G_IN_ROOM | G_OWNER,       cmd_add_item    },
    { CMD_REMOVE_ITEM, 1, G_AUTH | G_IN_ROOM | G_OWNER,       cmd_remove_item },
    { CMD_BID,         2, G_AUTH | G_IN_ROOM | G_NOT_OWNER,   cmd_bid         },
    { CMD_BUY_NOW,     1, G_AUTH | G_IN_ROOM | G_NOT_OWNER,   cmd_buy_now     }
};

void cmd_reply(session_t *s, int code, const char *fmt, ...) {
    if (!s || s->replied) return;

    char msg[MAX_RESP_LINE - 16];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    session_send(s, "%d %s", code, msg);
    s->replied = 1;

    log_level_t lv = LOG_INFO;
    if (code >= 500) lv = LOG_ERROR;
    else if (code >= 400) lv = LOG_WARN;

    if (code < 400) {
        if (s->cur_log[0] != '\0') {
            logger_log(lv, session_subject(s), "%s %s OK", s->cur_cmd, s->cur_log);
        } else {
            logger_log(lv, session_subject(s), "%s OK", s->cur_cmd);
        }
    } else {
        if (s->cur_log[0] != '\0') {
            logger_log(lv, session_subject(s), "%s %s REJECT(%d)", s->cur_cmd, s->cur_log, code);
        } else {
            logger_log(lv, session_subject(s), "%s REJECT(%d)", s->cur_cmd, code);
        }
    }
}

static void build_log_args(session_t *s, const char *cmd_name, int argc, char *argv[]) {
    s->cur_log[0] = '\0';
    if (argc <= 0) return;

    if (strcmp(cmd_name, CMD_REGISTER) == 0 || strcmp(cmd_name, CMD_LOGIN) == 0) {
        snprintf(s->cur_log, sizeof(s->cur_log), "%s", argv[0]);
        return;
    }

    size_t pos = 0;
    for (int i = 0; i < argc; i++) {
        int n = snprintf(s->cur_log + pos, sizeof(s->cur_log) - pos,
                         "%s%s", (i > 0 ? " " : ""), argv[i]);
        if (n < 0) break;
        if ((size_t)n >= sizeof(s->cur_log) - pos) {
            pos = sizeof(s->cur_log) - 1;
            break;
        }
        pos += (size_t)n;
    }
}

void handler_process_line(session_t *s, char *line) {
    if (!s || !line) return;
    s->replied = 0;
    snprintf(s->cur_cmd, sizeof(s->cur_cmd), "UNKNOWN");
    s->cur_log[0] = '\0';

    char *tokens[MAX_TOKENS];
    int ntok = parse_tokenize(line, tokens, MAX_TOKENS);
    if (ntok == 0) return; /* dòng toàn dấu cách */

    if (ntok == -1) {
        cmd_reply(s, RC_BAD_REQUEST, "Unterminated quote");
        return;
    }
    if (ntok == -2) {
        cmd_reply(s, RC_BAD_REQUEST, "Wrong number of arguments");
        return;
    }

    const char *cmd_name = tokens[0];
    int argc = ntok - 1;
    char **argv = &tokens[1];

    snprintf(s->cur_cmd, sizeof(s->cur_cmd), "%s", cmd_name);
    build_log_args(s, cmd_name, argc, argv);

    /* Bước 1: tra bảng lệnh */
    const command_t *cmd = NULL;
    size_t ncmds = sizeof(g_commands) / sizeof(g_commands[0]);
    for (size_t i = 0; i < ncmds; i++) {
        if (strcmp(g_commands[i].name, cmd_name) == 0) {
            cmd = &g_commands[i];
            break;
        }
    }
    if (!cmd) {
        cmd_reply(s, RC_BAD_REQUEST, "Unknown command");
        return;
    }

    /* Bước 2: Guard đăng nhập */
    if ((cmd->guards & G_AUTH) && s->state == SS_CONNECTED) {
        cmd_reply(s, RC_UNAUTHORIZED, "Not logged in");
        return;
    }
    if ((cmd->guards & G_NOT_AUTH) && s->state != SS_CONNECTED) {
        cmd_reply(s, RC_CONFLICT, "Already logged in");
        return;
    }

    /* Bước 3: Guard trong phòng / ngoài phòng */
    if ((cmd->guards & G_IN_ROOM) && s->state != SS_IN_ROOM) {
        cmd_reply(s, RC_FORBIDDEN, "Not in a room");
        return;
    }
    if ((cmd->guards & G_NOT_IN_ROOM) && s->state == SS_IN_ROOM) {
        cmd_reply(s, RC_CONFLICT, "Already in a room");
        return;
    }

    /* Bước 4: Guard vai trò chủ phòng / thành viên */
    if (cmd->guards & (G_OWNER | G_NOT_OWNER)) {
        room_t *r = room_find(s->room_id);
        int is_owner = (r && strcmp(r->owner, s->username) == 0) ? 1 : 0;
        if ((cmd->guards & G_OWNER) && !is_owner) {
            cmd_reply(s, RC_FORBIDDEN, "Only room owner can do this");
            return;
        }
        if ((cmd->guards & G_NOT_OWNER) && is_owner) {
            cmd_reply(s, RC_FORBIDDEN, "Room owner cannot bid");
            return;
        }
    }

    /* Bước 5: Kiểm tra số lượng tham số */
    if (argc != cmd->argc) {
        cmd_reply(s, RC_BAD_REQUEST, "Wrong number of arguments");
        return;
    }

    cmd->fn(s, argc, argv);

    if (!s->replied) {
        cmd_reply(s, RC_INTERNAL, "Internal error");
    }
}

int cmd_register(session_t *s, int argc, char *argv[]) {
    (void)argc;
    const char *username = argv[0];
    const char *password = argv[1];

    if (!parse_valid_username(username)) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid username");
        return 0;
    }
    if (!parse_valid_password(password)) {
        cmd_reply(s, RC_BAD_REQUEST, "Invalid password");
        return 0;
    }

    int rc = account_register(username, password);
    if (rc == ACC_ERR_EXISTS) {
        cmd_reply(s, RC_CONFLICT, "Username already exists");
        return 0;
    }
    if (rc != ACC_OK) {
        cmd_reply(s, RC_INTERNAL, "Server storage full");
        return 0;
    }

    cmd_reply(s, RC_CREATED, "Registered %s", username);
    return 0;
}

int cmd_login(session_t *s, int argc, char *argv[]) {
    (void)argc;
    const char *username = argv[0];
    const char *password = argv[1];

    if (!parse_valid_username(username) || !parse_valid_password(password)) {
        cmd_reply(s, RC_UNAUTHORIZED, "Invalid username or password");
        return 0;
    }
    if (!account_verify(username, password)) {
        cmd_reply(s, RC_UNAUTHORIZED, "Invalid username or password");
        return 0;
    }
    if (session_find_by_user(username) != NULL) {
        cmd_reply(s, RC_CONFLICT, "Account is logged in elsewhere");
        return 0;
    }

    snprintf(s->username, sizeof(s->username), "%s", username);
    s->state = SS_AUTH;
    s->room_id = 0;

    /* Ghi log dưới ip:port hoặc username — đã gán username nên log sẽ mang username,
       nhưng spec 04 §3.2 cho phép ip:port LOGIN alice OK hoặc alice LOGIN.
       Để khớp ví dụ spec 04 §3.2: tạm giữ addr khi gọi cmd_reply rồi mới bật state,
       hoặc gán sau cmd_reply. */
    s->state = SS_CONNECTED;
    cmd_reply(s, RC_OK, "Login successful");
    s->state = SS_AUTH;
    return 0;
}

int cmd_logout(session_t *s, int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    if (s->state == SS_IN_ROOM && s->room_id > 0) {
        room_leave(s);
    }
    cmd_reply(s, RC_OK, "Logged out");
    s->username[0] = '\0';
    s->room_id = 0;
    s->state = SS_CONNECTED;
    return 0;
}
