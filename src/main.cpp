#include "ramfb.h"
#include "Settings.h"
#include <stdint.h>

#define fb_loc 0x40002800

extern "C" void start() {
    QemuRamFBCfg ramfb_cfg = {
        .address = BE64(fb_loc),
        .fourcc = BE32(FORMAT_XRGB8888),
        .flags = BE32(0),
        .width = BE32(WIDTH),
        .height = BE32(HEIGHT),
        .stride = BE32(4 * WIDTH)
    };
    
    qemu_ramfb_configure(&ramfb_cfg);

    uint32_t* fb_address = (uint32_t*)fb_loc;
    for (int i = 0; i < WIDTH*HEIGHT; i++) {
        fb_address[i] = 0xFFFF0000;
    }
}