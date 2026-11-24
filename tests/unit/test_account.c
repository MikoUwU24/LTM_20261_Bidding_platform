#include "server/account.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char h1[17], h2[17], h3[17];
    account_hash("0123456789abcdef", "secret1", h1);
    account_hash("0123456789abcdef", "secret1", h2);
    account_hash("fedcba9876543210", "secret1", h3);

    assert(strlen(h1) == 16);
    assert(strcmp(h1, h2) == 0);
    assert(strcmp(h1, h3) != 0);

    printf("PASS unit/test_account\n");
    return 0;
}
