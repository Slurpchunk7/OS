#include "../memory/heap.h"
#include "../utils/math.h"

#define STBTT_memcpy memcpy
#define STBTT_memset memset

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u)  kmalloc(x)
#define STBTT_free(x, u)    kfree(x)
#define STBTT_assert(x)     // no assert in bare metal

#include "stb_truetype.h"
#include "font.h"
#include "../fs/fat32.h"
#include "../print.h"
#include "../Settings.h"

static stbtt_fontinfo font_info;
static uint8_t*       font_data = 0;
static bool           font_ready = false;

bool font_init(const char* path) {
    fat_entry_t f;
    if (!fat32_open(path, &f)) {
        uart_puts("font_init: file not found: ");
        uart_puts(path);
        uart_putc('\n');
        return false;
    }

    font_data = (uint8_t*)kmalloc(f.size);
    if (!font_data) {
        uart_puts("font_init: alloc failed\n");
        return false;
    }

    fat32_read(&f, font_data);

    if (!stbtt_InitFont(&font_info, font_data, 0)) {
        uart_puts("font_init: stbtt_InitFont failed\n");
        kfree(font_data);
        font_data = 0;
        return false;
    }

    font_ready = true;
    uart_puts("font_init: loaded ");
    uart_puts(path);
    uart_putc('\n');
    return true;
}

void font_draw_text(uint32_t* fb, const char* text, int x, int y, float size, uint32_t color) {
    if (!font_ready) return;

    float scale = stbtt_ScaleForPixelHeight(&font_info, size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
    int baseline = (int)(ascent * scale);

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b = (color >>  0) & 0xFF;

    int cx = x;
    for (const char* p = text; *p; p++) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font_info, *p, &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font_info, *p, scale, scale, &x0, &y0, &x1, &y1);

        int bw = x1 - x0;
        int bh = y1 - y0;

        uint8_t* bitmap = (uint8_t*)kmalloc(bw * bh);
        if (!bitmap) continue;

        stbtt_MakeCodepointBitmap(&font_info, bitmap, bw, bh, bw, scale, scale, *p);

        for (int row = 0; row < bh; row++) {
            for (int col = 0; col < bw; col++) {
                uint8_t alpha = bitmap[row * bw + col];
                if (alpha == 0) continue;

                int px = cx + x0 + col;
                int py = y + y0 + row;
                if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT) continue;

                // alpha blend with existing pixel
                uint32_t dst = fb[py * WIDTH + px];
                uint8_t dr = (dst >> 16) & 0xFF;
                uint8_t dg = (dst >>  8) & 0xFF;
                uint8_t db = (dst >>  0) & 0xFF;

                uint8_t a = alpha;
                uint8_t nr = (r * a + dr * (255 - a)) / 255;
                uint8_t ng = (g * a + dg * (255 - a)) / 255;
                uint8_t nb = (b * a + db * (255 - a)) / 255;

                fb[py * WIDTH + px] = ((uint32_t)nr << 16) | ((uint32_t)ng << 8) | nb;
            }
        }

        kfree(bitmap);

        // kerning
        int kern = stbtt_GetCodepointKernAdvance(&font_info, *p, *(p+1));
        cx += (int)((advance + kern) * scale);
    }
}

int font_text_width(const char* text, float size) {
    if (!font_ready) return 0;

    float scale = stbtt_ScaleForPixelHeight(&font_info, size);
    int width = 0;

    for (const char* p = text; *p; p++) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font_info, *p, &advance, &lsb);
        int kern = stbtt_GetCodepointKernAdvance(&font_info, *p, *(p+1));
        width += (int)((advance + kern) * scale);
    }
    return width;
}