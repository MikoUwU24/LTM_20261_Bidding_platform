#include "server/account.h"
#include "common/parse.h"
#include "server/logger.h"
#include "server/storage.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static account_t g_accounts[MAX_ACCOUNTS];
static int       g_account_count = 0;

void account_hash(const char *salt, const char *password, char out_hex[17]) {
    uint64_t h = 0xcbf29ce484222325ULL;
    size_t slen = salt ? strlen(salt) : 0;
    size_t plen = password ? strlen(password) : 0;

    for (int r = 0; r < HASH_ROUNDS; r++) {
        for (size_t i = 0; i < slen; i++) {
            h ^= (unsigned char)salt[i];
            h *= 0x100000001b3ULL;
        }
        for (size_t i = 0; i < plen; i++) {
            h ^= (unsigned char)password[i];
            h *= 0x100000001b3ULL;
        }
    }
    snprintf(out_hex, 17, "%016llx", (unsigned long long)h);
}

static void generate_salt(char out_salt[SALT_LEN + 1]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < SALT_LEN; i++) {
        out_salt[i] = hex[rand() % 16];
    }
    out_salt[SALT_LEN] = '\0';
}

static account_t *find_account(const char *username) {
    for (int i = 0; i < g_account_count; i++) {
        if (strcmp(g_accounts[i].username, username) == 0) {
            return &g_accounts[i];
        }
    }
    return NULL;
}

static int account_line_cb(char *line, void *ctx) {
    (void)ctx;
    char *tokens[4];
    int n = parse_tokenize(line, tokens, 4);
    if (n != 3) {
        logger_log(LOG_WARN, "server", "STORAGE invalid account line");
        return 0;
    }
    if (!parse_valid_username(tokens[0]) ||
        strlen(tokens[1]) != SALT_LEN ||
        strlen(tokens[2]) != 16) {
        logger_log(LOG_WARN, "server", "STORAGE malformed account entry");
        return 0;
    }
    if (g_account_count >= MAX_ACCOUNTS) {
        logger_log(LOG_WARN, "server", "STORAGE max accounts reached");
        return 0;
    }
    if (find_account(tokens[0]) != NULL) return 0;

    account_t *a = &g_accounts[g_account_count++];
    snprintf(a->username, sizeof(a->username), "%s", tokens[0]);
    snprintf(a->salt, sizeof(a->salt), "%s", tokens[1]);
    snprintf(a->hash, sizeof(a->hash), "%s", tokens[2]);
    return 0;
}

static int account_writer_cb(FILE *fp, void *ctx) {
    (void)ctx;
    for (int i = 0; i < g_account_count; i++) {
        if (fprintf(fp, "%s %s %s\n",
                    g_accounts[i].username,
                    g_accounts[i].salt,
                    g_accounts[i].hash) < 0) {
            return -1;
        }
    }
    return 0;
}

int account_load(void) {
    g_account_count = 0;
    return storage_read_lines(ACCOUNTS_FILE, account_line_cb, NULL);
}

int account_register(const char *username, const char *password) {
    if (!username || !password) return ACC_ERR_IO;
    if (find_account(username) != NULL) return ACC_ERR_EXISTS;
    if (g_account_count >= MAX_ACCOUNTS) return ACC_ERR_FULL;

    account_t *a = &g_accounts[g_account_count];
    snprintf(a->username, sizeof(a->username), "%s", username);
    generate_salt(a->salt);
    account_hash(a->salt, password, a->hash);
    g_account_count++;

    if (storage_write_file(ACCOUNTS_FILE, account_writer_cb, NULL) < 0) {
        g_account_count--;
        logger_log(LOG_ERROR, "server", "STORAGE write %s failed: %s",
                   ACCOUNTS_FILE, strerror(errno));
        return ACC_ERR_IO;
    }
    return ACC_OK;
}

int account_verify(const char *username, const char *password) {
    if (!username || !password) return 0;
    account_t *a = find_account(username);
    if (!a) return 0;
    char computed[17];
    account_hash(a->salt, password, computed);
    return strcmp(computed, a->hash) == 0 ? 1 : 0;
}
