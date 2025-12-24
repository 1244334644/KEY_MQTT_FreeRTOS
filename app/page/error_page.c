#include <stdint.h>
#include <string.h>
#include "lcd.h"
#include "img.h"
#include "font.h"
#include "board.h"
#include "ui.h"

void error_page_display(lcd_desc_t lcd, const char *msg)
{
    const uint16_t color_bg = mkcolor(0,0,0);//黑色
    ui_fill_color(lcd, 0, 0, 239, 319, color_bg);//黑色背景
    ui_draw_image(lcd, 40, 37, &img_error);
    int len = strlen(msg)*font20.size/2;
    uint16_t x = 0;
    if(len<240)
        x = (240 - len + 1) / 2;
    else
        x = 0;
    ui_write_string(lcd, x, 245, msg, mkcolor(255,255,0),color_bg, &font20);
}

