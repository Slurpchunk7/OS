#pragma once

#include <stdint.h>

#include "types/string.h"

void draw_char_bmp(uint32_t* fb, char c, uint32_t x, uint32_t y, uint32_t color, uint32_t size);

void draw_text_bmp(uint32_t* fb, const char* text, uint32_t x, uint32_t y, uint32_t color, uint32_t size);

void draw_text_bmp(uint32_t* fb, const String& text, uint32_t x, uint32_t y, uint32_t color, uint32_t size);