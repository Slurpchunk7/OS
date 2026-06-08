#include "ramfb.h"
#include "Settings.h"
#include "text.h"
#include "pci.h"
#include "print.h"


#include <stdint.h>

#define fb_loc 0x40002800

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

        draw_text(fb, "Hello OS!", 100, 100, 0xFFFFFFFF, 2);

        for (volatile int i = 0; i < 1000000; i++);
    }
}