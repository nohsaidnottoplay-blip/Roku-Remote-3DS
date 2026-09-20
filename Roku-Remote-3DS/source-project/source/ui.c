#include "ui.h"
#include <string.h>
#include <ctype.h>

/* Original 5 by 7 uppercase bitmap alphabet. */
static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .:-/+<>*!?()=";
static const unsigned char letters[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},
 {0,0,0,0,0,0,0},{0,0,0,0,0,6,6},{0,6,6,0,6,6,0},
 {0,0,0,31,0,0,0},{1,2,2,4,8,8,16},{0,4,4,31,4,4,0},
 {2,4,8,16,8,4,2},{8,4,2,1,2,4,8},{0,21,14,31,14,21,0},
 {4,4,4,4,4,0,4},{14,17,1,2,4,0,4},{2,4,8,8,8,4,2},
 {8,4,2,2,2,4,8},{0,0,31,0,31,0,0}
};

static void pixel(Canvas c, int x, int y, uint32_t color) {
    if ((unsigned)x >= (unsigned)c.width || (unsigned)y >= (unsigned)c.height) return;
    unsigned char *p = c.pixels + (x * c.height + c.height - 1 - y) * 3;
    p[0] = (unsigned char)color;
    p[1] = (unsigned char)(color >> 8);
    p[2] = (unsigned char)(color >> 16);
}

void ui_rect(Canvas c, int x, int y, int w, int h, uint32_t color) {
    for (int xx = x; xx < x + w; ++xx)
        for (int yy = y; yy < y + h; ++yy) pixel(c, xx, yy, color);
}

void ui_clear(Canvas c, uint32_t color) { ui_rect(c, 0, 0, c.width, c.height, color); }

void ui_round(Canvas c, int x, int y, int w, int h, int r, uint32_t color) {
    for (int xx = 0; xx < w; ++xx) for (int yy = 0; yy < h; ++yy) {
        int dx = xx < r ? r - xx : xx >= w - r ? xx - (w - r - 1) : 0;
        int dy = yy < r ? r - yy : yy >= h - r ? yy - (h - r - 1) : 0;
        if (dx * dx + dy * dy <= r * r) pixel(c, x + xx, y + yy, color);
    }
}

void ui_text(Canvas c, int x, int y, int scale, uint32_t color, const char *s) {
    for (; *s; ++s, x += 6 * scale) {
        char ch = (char)toupper((unsigned char)*s);
        const char *p = strchr(alphabet, ch);
        if (!p) continue;
        const unsigned char *glyph = letters[p - alphabet];
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
            if (glyph[row] & (1 << (4 - col)))
                ui_rect(c, x + col * scale, y + row * scale, scale, scale, color);
    }
}

void ui_wrap(Canvas c, int x, int y, int width, int scale, uint32_t color, const char *s) {
    int limit = width / (6 * scale);
    if (limit < 1) return;
    if (limit > 95) limit = 95;
    while (*s && y + 7 * scale <= c.height) {
        while (*s == ' ') ++s;
        int n = 0, space = -1;
        while (s[n] && s[n] != '\n' && n < limit) {
            if (s[n] == ' ') space = n;
            ++n;
        }
        if (n == limit && s[n] && s[n] != '\n' && space > 0) n = space;
        char line[96];
        memcpy(line, s, (size_t)n);
        line[n] = 0;
        ui_text(c, x, y, scale, color, line);
        s += n;
        if (*s == '\n') ++s;
        y += 9 * scale;
    }
}

void ui_button(Canvas c, Button b, int selected) {
    ui_round(c, b.x, b.y + 2, b.w, b.h, 7, 0x0F1021);
    ui_round(c, b.x, b.y, b.w, b.h - 1, 7, selected ? 0x9764E8 : 0x343047);
    int scale = (int)strlen(b.label) * 12 <= b.w - 8 && b.h >= 28 ? 2 : 1;
    int width = ((int)strlen(b.label) * 6 - 1) * scale;
    ui_text(c, b.x + (b.w - width) / 2, b.y + (b.h - 7 * scale) / 2, scale, 0xFFFFFF, b.label);
}

int ui_hit(Button b, int x, int y) {
    return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h;
}
