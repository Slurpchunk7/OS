#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_putc(char c);

void uart_puts(const char* s);

void print_int(int value);

void print(const char* fmt, ...);

void print_hex(uint64_t hex);

void print_dec(uint32_t value);

#ifdef __cplusplus
}
#endif