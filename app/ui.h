#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include "font.h"
#include "img.h"
#include "board.h"

void ui_init(lcd_desc_t lcd);
void ui_fill_color(lcd_desc_t lcd, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ui_write_string(lcd_desc_t lcd, uint16_t x, uint16_t y,const char *str, uint16_t color, uint16_t bg_color,const font_t *font);
void ui_draw_image(lcd_desc_t lcd, uint16_t x, uint16_t y, const img_t *img);

#endif // __UI_H__
