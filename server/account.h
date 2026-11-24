#ifndef SERVER_ACCOUNT_H
#define SERVER_ACCOUNT_H

#include "common/protocol.h"

typedef struct {
    char username[MAX_USERNAME + 1];
    char salt[SALT_LEN + 1];
    char hash[17];
} account_t;

int  account_load(void);
int  account_register(const char *username, const char *password);
int  account_verify(const char *username, const char *password);
void account_hash(const char *salt, const char *password, char out_hex[17]);

enum {
    ACC_OK         = 0,
    ACC_ERR_EXISTS = -1,
    ACC_ERR_FULL   = -2,
    ACC_ERR_IO     = -3
};

#endif /* SERVER_ACCOUNT_H */
