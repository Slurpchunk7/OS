#pragma once

#include <stdint.h>
#include "../Settings.h"

typedef struct { int x, y, w, h; } rect_t;

static inline void flush_rect(uint32_t* dst, uint32_t* src, rect_t r) {
    for (int y = r.y; y < r.y + r.h; y++) {
        int off = y * WIDTH + r.x;
        for (int x = 0; x < r.w; x++) {
            dst[off + x] = src[off + x];
        }
    }
}

static inline void draw_rect(uint32_t* fb, rect_t r, uint32_t color) {
    for (int y = r.y; y < r.y + r.h; y++) {
        int off = y * WIDTH + r.x;
        for (int x = 0; x < r.w; x++) {
            fb[off + x] = color;
        }
    }
}

static inline void draw_image(uint32_t* fb, const uint32_t* bitmap, int x, int y, int w, int h) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint32_t pixel = bitmap[row * w + col];
            uint8_t a = (pixel >> 24) & 0xFF;
            if (a == 0) continue; // skip transparent
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT) continue;
            fb[py * WIDTH + px] = pixel & 0x00FFFFFF; // strip alpha, XRGB8888
        }
    }
}