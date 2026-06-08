#include "print.h"

#define UART0_BASE 0x09000000UL
#define UART0_DR ((volatile unsigned int*)(UART0_BASE + 0x00))
#define UART0_FR ((volatile unsigned int*)(UART0_BASE + 0x18))

void uart_putc(char c)
{
    while (*UART0_FR & (1 << 5))
    {
        // TX FIFO full
    }

    *UART0_DR = (unsigned int)c;
}

void uart_puts(const char* s)
{
    while (*s)
    {
        uart_putc(*s++);
    }
}

void print(const char* text)
{
    uart_puts(text);
}

void print_hex(uint64_t value)
{
    const char* hex = "0123456789ABCDEF";

    uart_puts("0x");

    int started = 0;

    for (int i = 15; i >= 0; i--)
    {
        uint8_t nibble = (value >> (i * 4)) & 0xF;

        if (nibble != 0 || i == 0)
            started = 1;

        if (started)
            uart_putc(hex[nibble]);
    }
}