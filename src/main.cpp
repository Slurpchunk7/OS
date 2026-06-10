#include "ramfb.h"
#include "Settings.h"
#include "text.h"
#include "pci.h"
#include "print.h"
#include "time.h"
#include "virtio_input.h"
#include "utils/keyboard.h"
#include <stdint.h>

#define fb_loc 0x40002800

const char* char_to_str(char c) {
    static char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    return buf; 
} 

extern "C" void start() {
    print("scanning\n");
    pci_scan();
    print("done\n");
    
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

    virtio_input_init(0, 2, 0);
    char last = '?';

    while (1)
    {
        virtio_input_event_t ev;
        if (virtio_input_poll(&ev)){
            if (ev.type == EV_KEY && ev.value == 1) {
                print("key: %d\n", ev.code);
                last = keycode_to_ascii(ev.code);
                uart_putc(keycode_to_ascii(ev.code));
                uart_putc('\n');
            }
        }
        
        uint32_t h, m, s;
        get_time(&h, &m, &s);

        static int frame = 0;
        frame++;

        uint32_t color = (frame % 2 == 0) ? 0xFF0000 : 0x0000FF;
        for (int i = 0; i < WIDTH * HEIGHT; i++) backbuffer[i] = color;

        for (int i = 0; i < WIDTH * HEIGHT; i++) fb[i] = backbuffer[i];
        
        for (volatile int i = 0; i < 1000000; i++); 
    }
}