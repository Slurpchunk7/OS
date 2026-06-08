#include "ramfb.h"
#include "Settings.h"
#include "text.h"
#include "pci.h"
#include "print.h"
#include "time.h"

#include <stdint.h>

#define fb_loc 0x40002800

void uart_print_dec(uint32_t value)
{
    char buf[11];
    int i = 0;

    if (value == 0)
    {
        uart_putc('0');
        return;
    }

    while (value > 0)
    {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i--)
    {
        uart_putc(buf[i]);
    }
}

void get_time(uint32_t t, uint32_t* h, uint32_t* m, uint32_t* s)
{
    *s = t % 60;
    t /= 60;

    *m = t % 60;
    t /= 60;

    *h = t % 24;
}

extern "C" void start() {
    print("scanning\n");

    pci_scan();

    print("done\n");
    
    QemuRamFBCfg ramfb_cfg = {
        .address = BE64(fb_loc),
        .fourcc = BE32(FORMAT_XRGB8888),
        .flags = BE32(0),
        .width = BE32(WIDTH),
        .height = BE32(HEIGHT),
        .stride = BE32(4 * WIDTH)
    };
    
    qemu_ramfb_configure(&ramfb_cfg);

    uint32_t* fb = (uint32_t*)fb_loc;

    while (1)
    {
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            fb[i] = 0xFF000000;
        
        uint32_t time = rtc_time_tz(10);
        uint32_t h, m, s;
        get_time(time, &h, &m, &s);
        print("Time: %d:%d:%d", h, m, s);
        uart_putc('\n');
        
        draw_text(fb, "hi", 100, 100, 0xFFFFFFFF, 2);

        for (volatile int i = 0; i < 1000000; i++);
    }
}