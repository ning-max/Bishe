#pragma once

#include <stdint.h>

void uart_putc(char c);
void uart_puts(const char *s);
void uart_putu(uint32_t n);
void uart_puti(int32_t n);
