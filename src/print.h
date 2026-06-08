#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_putc(char c);

void uart_puts(const char* s);

void print(const char* text);

void print_hex(uint64_t hex);

#ifdef __cplusplus
}
#endif