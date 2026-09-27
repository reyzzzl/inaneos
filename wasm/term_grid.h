#pragma once

void term_init(void);
void term_clear(void);
void term_putc(char c);
void term_puts(const char *s);
void term_write(const char *s, unsigned long len);
void term_goto(int r, int c);
void term_set_color(int fg, int bg);

unsigned short *term_cells(void); // cells
int term_cursor(void); // cursor
int term_cols(void);
int term_rows(void);
