#pragma once
#include <stdint.h>
#include "../Settings.h"

typedef struct { int x, y, w, h; } rect_t;

static inline void flush_rect(uint32_t* dst, uint32_t* src, rect_t r) {
    for (int y = r.y; y < r.y; y++) {
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