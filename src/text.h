#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void draw_char(uint32_t* fb, char c, uint32_t x, uint32_t y, uint32_t color, uint32_t size);

void draw_text(uint32_t* fb, const char* text, uint32_t x, uint32_t y, uint32_t color, uint32_t size);

#ifdef __cplusplus
}
#endif