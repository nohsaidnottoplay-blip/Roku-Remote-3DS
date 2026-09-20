#ifndef ROKU_UI_H
#define ROKU_UI_H
#include <stdint.h>
typedef struct { unsigned char *pixels; int width, height; } Canvas;
typedef struct { int x, y, w, h; const char *label; const char *key; } Button;
void ui_clear(Canvas c, uint32_t color);
void ui_rect(Canvas c, int x, int y, int w, int h, uint32_t color);
void ui_round(Canvas c, int x, int y, int w, int h, int radius, uint32_t color);
void ui_text(Canvas c, int x, int y, int scale, uint32_t color, const char *s);
void ui_wrap(Canvas c, int x, int y, int width, int scale, uint32_t color, const char *s);
void ui_button(Canvas c, Button b, int selected);
int ui_hit(Button b, int x, int y);
#endif
