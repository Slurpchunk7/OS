#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../Settings.h"
#include "../utils/rect.h"
#include "../fonts/font.h"

typedef struct {
    int x, y, w, h;
    char title[64];
    bool focused;
    bool dragging;
    int drag_off_x, drag_off_y;
} window_t;

#define TITLEBAR_HEIGHT 28
#define BORDER_COLOR    0x555555
#define TITLEBAR_COLOR  0x3A3A3A
#define BG_COLOR        0x1E1E1E

static inline void window_draw(uint32_t* fb, window_t* win) {
    rect_t bg = { win->x + 1, win->y + TITLEBAR_HEIGHT + 1, win->w + 1, win->h - TITLEBAR_HEIGHT + 1};
    draw_rect(fb, bg, BG_COLOR);

    rect_t tb = { win->x, win->y, win->w, TITLEBAR_HEIGHT };
    draw_rect(fb, tb, win->focused ? TITLEBAR_COLOR : BORDER_COLOR);

    rect_t bt = { win->x, win->y, win->w, 1 };
    draw_rect(fb, bt, BORDER_COLOR);

    rect_t bb = { win->x, win->y + win->h, win->w, 1 };
    draw_rect(fb, bb, BORDER_COLOR);

    rect_t bl = { win->x, win->y, 1, win->h };
    draw_rect(fb, bl, BORDER_COLOR);

    rect_t br = { win->x + win->w - 1, win->y, 1, win->h };
    draw_rect(fb, br, BORDER_COLOR);

    font_draw_text(fb, win->title, win->x + 10, win->y + (TITLEBAR_HEIGHT / 2)+TITLEBAR_HEIGHT/5, TITLEBAR_HEIGHT - 5, 0xFFFFFFFF);
}

static inline bool window_hit_titlebar(window_t* win, int mx, int my) {
    return mx >= win->x && mx < win->x + win->w &&
           my >= win->y && my < win->y + TITLEBAR_HEIGHT;
}

static inline void window_on_mouse_down(window_t* win, int mx, int my) {
    if (window_hit_titlebar(win, mx, my)) {
        win->dragging    = true;
        win->focused     = true;
        win->drag_off_x  = mx - win->x;
        win->drag_off_y  = my - win->y;
    }
}

static inline void window_on_mouse_up(window_t* win) {
    win->dragging = false;
}

static inline void window_on_mouse_move(window_t* win, int mx, int my) {
    if (!win->dragging) return;
    win->x = mx - win->drag_off_x;
    win->y = my - win->drag_off_y;

    // clamp to screen
    if (win->x < 0) win->x = 0;
    if (win->y < 0) win->y = 0;
    if (win->x + win->w > WIDTH)  win->x = WIDTH  - win->w;
    if (win->y + win->h > HEIGHT) win->y = HEIGHT - win->h;
}