#pragma once

#include <stdint.h>

#define SWAP16(x) __builtin_bswap16(x)
#define SWAP32(x) __builtin_bswap32(x)
#define SWAP64(x) __builtin_bswap64(x)

#define PIXEL_FORMAT_RGB888    875710290
#define PIXEL_FORMAT_XRGB8888  875713112
#define PIXEL_FORMAT_RGB565    909199186

struct QemuRamFBCfg {
    uint64_t address;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} __attribute__((packed));

#ifdef __cplusplus
extern "C" {
#endif

void qemu_ramfb_make_cfg(struct QemuRamFBCfg* cfg, void* fb_address, uint32_t fb_width, uint32_t fb_height);
void qemu_ramfb_configure(struct QemuRamFBCfg* cfg);

void set_pixel(uint32_t* fb, uint32_t x, uint32_t y, uint32_t width, uint32_t color);

#ifdef __cplusplus
}
#endif