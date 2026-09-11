// vga.h - screen api

#pragma once

void term_init(void);
void term_clear(void);
void term_putc(char c);
void term_puts(const char *s);
void term_set_color(unsigned char fg, unsigned char bg);
