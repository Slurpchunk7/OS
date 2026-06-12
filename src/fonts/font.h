#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool font_init(const char* path);
void font_draw_text(uint32_t* fb, const char* text, int x, int y, float size, uint32_t color);
int  font_text_width(const char* text, float size);

#ifdef __cplusplus
}
#endif