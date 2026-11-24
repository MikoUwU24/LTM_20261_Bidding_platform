#ifndef COMMON_PARSE_H
#define COMMON_PARSE_H

#include <stddef.h>

/* Tách token theo spec 02 §2. line bị sửa tại chỗ (chèn '\0').
   Trả số token (≥0), hoặc -1 sai cú pháp (ngoặc kép), -2 quá MAX_TOKENS. */
int  parse_tokenize(char *line, char *tokens[], int max_tokens);
/* Chuỗi chỉ gồm chữ số, giá trị trong [min, max]. 0 = OK, -1 = sai. */
int  parse_ll(const char *s, long long min, long long max, long long *out);
int  parse_valid_username(const char *s);
int  parse_valid_password(const char *s);
/* Chuỗi in được, không '"', không byte < 0x20, độ dài trong [min_len, max_len]. */
int  parse_valid_text(const char *s, size_t min_len, size_t max_len);
/* Tách danh sách "; " bên ngoài ngoặc kép (client dùng). Trả số đoạn. */
int  parse_split_records(char *line, char *records[], int max_records);

#endif /* COMMON_PARSE_H */
