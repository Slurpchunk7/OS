#include "print.h"

#include <stdarg.h>

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

void print_int(int value)
{
    char buf[32];
    int i = 30;
    buf[31] = '\0';

    int neg = 0;
    if (value < 0)
    {
        neg = 1;
        value = -value;
    }

    if (value == 0)
    {
        uart_putc('0');
        return;
    }

    while (value > 0 && i >= 0)
    {
        buf[i--] = '0' + (value % 10);
        value /= 10;
    }

    if (neg)
        buf[i--] = '-';

    uart_puts(&buf[i + 1]);
}

void print(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt == '%' && *(fmt + 1))
        {
            fmt++;

            switch (*fmt)
            {
                case 's':
                    uart_puts(va_arg(args, const char*));
                    break;

                case 'd':
                    print_int(va_arg(args, int));
                    break;

                case 'x':
                    print_hex(va_arg(args, uint32_t));
                    break;

                case 'c':
                    uart_putc((char)va_arg(args, int));
                    break;

                case '%':
                    uart_putc('%');
                    break;

                default:
                    uart_putc('%');
                    uart_putc(*fmt);
                    break;
            }
        }
        else
        {
            uart_putc(*fmt);
        }

        fmt++;
    }

    va_end(args);
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