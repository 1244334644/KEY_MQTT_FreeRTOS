#include <stdint.h>
#include <string.h>
#include "lcd.h"
#include "img.h"
#include "font.h"
#include "board.h"
#include "loop.h"
#include "ui.h"

void wifi_page_display(lcd_desc_t lcd)
{
    static const char *ssid = WIFI_SSID;
    uint16_t x = 0;
    int ssid_len = strlen(ssid)*font20.size/2;
    if(ssid_len < 240)
    {
        x = (240 - ssid_len+1) / 2;
    }

    const uint16_t color_bg = mkcolor(0,0,0);//黑色
    ui_fill_color(lcd, 0, 0, 239, 319, color_bg);//白色背景
    ui_draw_image(lcd, 30, 15, &img_wifi);
    ui_write_string(lcd, 88, 191, "WiFi",mkcolor(0,255,234) ,color_bg ,&font32);
    ui_write_string(lcd, x, 231, ssid,mkcolor(255,255,255) ,color_bg ,&font20);
    ui_write_string(lcd, 84, 263, "连接中",mkcolor(148,198,255) ,color_bg ,&font24);

}

