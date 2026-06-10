#include "ramfb.h"
#include "Settings.h"
#include "text.h"
#include "pci.h"
#include "print.h"
#include "time.h"
#include "virtio_input.h"
#include "utils/keyboard.h"
#include "utils/rect.h"
#include "memory/page_alloc.h"
#include "memory/heap.h"

#include "bmp/cursor.h"

#include <stdint.h>

#include <arm_neon.h>

#define fb_loc 0x40002800

const char* char_to_str(char c) {
    static char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    return buf; 
} 

void flush(uint32_t* dst, uint32_t* src, int count) {
    int i = 0;
    for (; i <= count - 4; i += 4) {
        uint32x4_t v = vld1q_u32(src + i);
        vst1q_u32(dst + i, v);
    }
    for (; i < count; i++) dst[i] = src[i];
}

extern "C" void start() {
    print("scanning\n");
    pci_scan();
    print("done\n");

    page_alloc_init();
    heap_init();
    
    QemuRamFBCfg ramfb_cfg = {
        .address = SWAP64(fb_loc),
        .fourcc = SWAP32(PIXEL_FORMAT_XRGB8888),
        .flags = SWAP32(0),
        .width = SWAP32(WIDTH),
        .height = SWAP32(HEIGHT),
        .stride = SWAP32(4 * WIDTH)
    };
    
    qemu_ramfb_configure(&ramfb_cfg);

    uint32_t* fb = (uint32_t*)fb_loc;
    static uint32_t backbuffer[WIDTH * HEIGHT];

    // keyboard
    virtio_input_init(0, 2, 0, 0);
    // mouse
    virtio_input_init(0, 3, 0, 1);
    int32_t mouse_x = 0;
    int32_t mouse_y = 0;

    char last = '?';

    while (1)
    {
        // keyboard
        virtio_input_event_t ev;
        if (virtio_input_poll(&ev, 0)){
            if (ev.type == EV_KEY && ev.value == 1) {
                print("key: %d\n", ev.code);
                last = keycode_to_ascii(ev.code);
                uart_putc(keycode_to_ascii(ev.code));
                uart_putc('\n');
            }
        }

        // mouse
        if (virtio_input_poll(&ev, 1)) {
            if (ev.type == EV_REL) {
                if (ev.code == 0) mouse_x += (int32_t)ev.value;
                if (ev.code == 1) mouse_y += (int32_t)ev.value;
            }
        }
        
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= WIDTH) mouse_x = WIDTH - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= HEIGHT) mouse_y = HEIGHT - 1;

        // cursor
        for (int i = 0; i < WIDTH * HEIGHT; i++) backbuffer[i] = 0;

        draw_image(backbuffer, cursor_t, mouse_x, mouse_y, 32, 32);

        flush(fb, backbuffer, WIDTH*HEIGHT);
 
        // for (volatile int i = 0; i < 1000000; i++); 
    }
}