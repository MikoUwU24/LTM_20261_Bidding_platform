#include "common/parse.h"
#include "common/protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_tokenize(void) {
    char line1[] = "  ADD_ITEM   \"Binh gom\"  \"\"   500000 2000000 ";
    char *tok[MAX_TOKENS];
    int n = parse_tokenize(line1, tok, MAX_TOKENS);
    assert(n == 5);
    assert(strcmp(tok[0], "ADD_ITEM") == 0);
    assert(strcmp(tok[1], "Binh gom") == 0);
    assert(strcmp(tok[2], "") == 0);
    assert(strcmp(tok[3], "500000") == 0);
    assert(strcmp(tok[4], "2000000") == 0);

    /* Thiếu ngoặc kép đóng */
    char line2[] = "CREATE_ROOM \"Do co";
    assert(parse_tokenize(line2, tok, MAX_TOKENS) == -1);

    /* Ngoặc kép dính chữ phía sau */
    char line3[] = "CREATE_ROOM \"Do co\"abc";
    assert(parse_tokenize(line3, tok, MAX_TOKENS) == -1);

    /* Vượt số token */
    char line4[] = "a b c d e f g h i";
    assert(parse_tokenize(line4, tok, MAX_TOKENS) == -2);
}

static void test_numbers_and_validators(void) {
    long long v = 0;
    assert(parse_ll("500000", 1, MAX_PRICE, &v) == 0 && v == 500000);
    assert(parse_ll("0", 1, MAX_PRICE, &v) == -1);
    assert(parse_ll("-10", 1, MAX_PRICE, &v) == -1);
    assert(parse_ll("12a", 1, MAX_PRICE, &v) == -1);
    assert(parse_ll("99999999999999999999999999", 1, MAX_PRICE, &v) == -1);

    assert(parse_valid_username("alice_01") == 1);
    assert(parse_valid_username("ab") == 0);
    assert(parse_valid_username("bo-b") == 0);

    assert(parse_valid_password("secret1") == 1);
    assert(parse_valid_password("abc") == 0);
    assert(parse_valid_password("bad pass") == 0);
    assert(parse_valid_password("bad\"pass") == 0);

    assert(parse_valid_text("Do co the ky 19", 1, 64) == 1);
    assert(parse_valid_text("", 0, 64) == 1);
    assert(parse_valid_text("", 1, 64) == 0);
    assert(parse_valid_text("Co \"ngoac\"", 1, 64) == 0);
}

static void test_split_records(void) {
    char line[] = "ROOMS 2; 1 alice \"Do; co\" 2 12 1; 2 bob \"Tranh\" 0 - 0";
    char *rec[8];
    int n = parse_split_records(line, rec, 8);
    assert(n == 3);
    assert(strcmp(rec[0], "ROOMS 2") == 0);
    assert(strcmp(rec[1], "1 alice \"Do; co\" 2 12 1") == 0);
    assert(strcmp(rec[2], "2 bob \"Tranh\" 0 - 0") == 0);
}

int main(void) {
    test_tokenize();
    test_numbers_and_validators();
    test_split_records();
    printf("PASS unit/test_parse\n");
    return 0;
}
