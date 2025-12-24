#ifndef __LCD_H__
#define __LCD_H__

#include <stdint.h>
#include <stdbool.h>
#include "font.h"
#include "img.h"
struct lcd_desc;

typedef struct lcd_desc *lcd_desc_t;
#define mkcolor(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

void lcd_init(lcd_desc_t lcd);
void st7789_fill_color(lcd_desc_t lcd, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void st7789_write_string(lcd_desc_t lcd, uint16_t x, uint16_t y,const char *str, uint16_t color, uint16_t bg_color,const font_t *font);
void st7789_draw_image(lcd_desc_t lcd, uint16_t x, uint16_t y, const img_t *img);


#endif /* __LCD_H__ */
