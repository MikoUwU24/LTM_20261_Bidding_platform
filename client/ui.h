#ifndef CLIENT_UI_H
#define CLIENT_UI_H

void ui_print_help(void);
void ui_print_server_line(const char *line);
/* Trả 1 = gửi `line`, 0 = không gửi (help/rỗng), -1 = thoát (quit). */
int  ui_translate_input(char *line);
void ui_prompt(void);

#endif /* CLIENT_UI_H */
