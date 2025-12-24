#include "lcd.h"
#include "img.h"
#include "font.h"
#include "board.h"
#include "ui.h"

void welcome_display(lcd_desc_t lcd)
{
    const uint16_t color_bg = mkcolor(0,0,0);//黑色
    ui_fill_color(lcd, 0, 0, 239, 319, color_bg);//白色背景
    ui_draw_image(lcd, 0, 0, &img_welcome);

}

