#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "img.h"
#include "font.h"
#include "board.h"
#include "loop.h"
#include "page.h"
#include "rtc.h"
#include "ui.h"

static const uint16_t color_bg_time = mkcolor(248,248,248);
static const uint16_t color_bg_inner = mkcolor(136,217,234);
static const uint16_t color_bg_outdoor = mkcolor(254,135,75);

void main_page_redraw_wifi_ssid(lcd_desc_t lcd, char *ssid);
void main_page_redraw_inner_temp(lcd_desc_t lcd,float temp);
void main_page_redraw_inner_humi(lcd_desc_t lcd, float humi);
void main_page_redraw_outdoor_city(lcd_desc_t lcd, const char *city);
void main_page_redraw_outdoor_temp(lcd_desc_t lcd,float temp);
void main_page_redraw_outdoor_weather_icon(lcd_desc_t lcd,const int code);

void main_page_display(lcd_desc_t lcd)
{


    ui_fill_color(lcd, 0, 0, 239, 319, mkcolor(0,0,0));//黑色背景

    do{
        ui_fill_color(lcd, 15, 15, 224, 154, color_bg_time);//白色矩形
        ui_draw_image(lcd, 23, 20, IMG_ICON_WIFI);//wifi图标
        main_page_redraw_wifi_ssid(lcd, WIFI_SSID);//SSID显示函数
        //时间显示
        ui_write_string(lcd, 25, 42, "--:--", mkcolor(0,0,0),color_bg_time, 76);//时间
        ui_write_string(lcd, 35, 121, "----/--/-- 星期四", mkcolor(143,143,143),color_bg_time, 20);//时间

    }while(0);
    do
    {
        ui_fill_color(lcd, 15, 165, 114, 304, color_bg_inner);//蓝色矩形
        ui_write_string(lcd, 19, 170, "室内环境", mkcolor(0,0,0),color_bg_inner, 20);//室内环境
        ui_write_string(lcd, 86, 191, "C", mkcolor(0,0,0),color_bg_inner, 32);//
        ui_write_string(lcd, 91, 262, "%", mkcolor(0,0,0),color_bg_inner, 32);//湿度

        main_page_redraw_inner_temp(lcd,999.9f);//室内温度显示函数，初始值不可用
        main_page_redraw_inner_humi(lcd,999.9f);//室内湿度显示函数，初始值不可用

    } while (0);
    
    
    do
    {
        ui_fill_color(lcd, 125, 165, 224, 304, color_bg_outdoor);//橙色矩形
        ui_write_string(lcd, 192, 189, "C", mkcolor(0,0,0),color_bg_outdoor, 32);//室外环境
        ui_draw_image(lcd, 139, 239, IMG_WENDUJI);//温度图标   
        main_page_redraw_outdoor_city(lcd, "广州");//城市显示函数，初始值不可用
        main_page_redraw_outdoor_temp(lcd,999.9f);//室外温度显示函数，初始值不可用
        
        main_page_redraw_outdoor_temp(lcd,999.9f);//室外温度显示函数，初始值不可用  
        main_page_redraw_outdoor_weather_icon(lcd,-1);//云量图标
    } while (0);
    
    
}


void main_page_redraw_wifi_ssid(lcd_desc_t lcd, char *ssid)
{
    char str[21];
    snprintf(str, sizeof(str), "%20s", ssid);
    ui_write_string(lcd, 50, 23, str, mkcolor(143,143,143),color_bg_time, 16);//SSID

}

void main_page_redraw_time(lcd_desc_t lcd, rtc_date_time_t *time)
{
    char str[6];
    char comma = (time->second % 2 == 0) ? ':' : ' ';
    snprintf(str, sizeof(str), "%02u%c%02u", time->hour, comma, time->minute);

    ui_write_string(lcd, 25, 42, str, mkcolor(0,0,0),color_bg_time, 76);//时间

}

void main_page_redraw_date(lcd_desc_t lcd, rtc_date_time_t *date)
{
    char str[18]; 
    snprintf(str, sizeof(str), "%04u/%02u/%02u 星期%s", date->year, date->month, date->day, 
    date->week == 1 ? "一" :
    date->week == 2 ? "二" :
    date->week == 3 ? "三" :
    date->week == 4 ? "四" :
    date->week == 5 ? "五" :
    date->week == 6 ? "六" :
    date->week == 7 ? "日" : "?");
    ui_write_string(lcd, 35, 121, str, mkcolor(143,143,143),color_bg_time, 20);//时间

}

void main_page_redraw_inner_temp(lcd_desc_t lcd,float temp)
{
    char str[3] = {'-','-'};
    if(temp>-10.0f && temp<100.0f)
        snprintf(str, sizeof(str), "%2.0f", temp);
    ui_write_string(lcd, 30, 192, str, mkcolor(0,0,0),color_bg_inner, 54);//室内温度

}

void main_page_redraw_inner_humi(lcd_desc_t lcd, float humi)
{
    char str[3];
    if(humi>0.0f && humi<99.99f)
        snprintf(str, sizeof(str), "%2.0f", humi);
    ui_write_string(lcd, 25, 239, str, mkcolor(0,0,0),color_bg_inner, 64);//室内湿度

}

void main_page_redraw_outdoor_city(lcd_desc_t lcd, const char *city)
{
    char str[9];
    snprintf(str, sizeof(str), "%s", city);
    ui_write_string(lcd, 127, 170, str, mkcolor(0,0,0),color_bg_outdoor, 20);//城市

}

void main_page_redraw_outdoor_temp(lcd_desc_t lcd,float temp)
{
    char str[3] = {'-','-'};
    if(temp>-10.0f && temp<100.0f)
        snprintf(str, sizeof(str), "%2.0f", temp);
    ui_write_string(lcd, 135, 190, str, mkcolor(0,0,0),color_bg_outdoor, 54);//室外温度

}

void main_page_redraw_outdoor_weather_icon(lcd_desc_t lcd,const int code)
{
    img_id_t icon;
    if(code==0||code==2||code==38)
        icon = IMG_QING;
    else if(code==1||code==3)
        icon = IMG_YUELIANG;
    else if(code==4||code==9)
        icon = IMG_YINTIAN;
    else if(code==5||code==6||code==7||code==8)
        icon = IMG_DUOYUN;
    else if(code==10||code==13||code==14||code==15||code==16||code==17||code==18||code==19)
        icon = IMG_ZHONGYU;
    else if(code==11||code==12)
        icon = IMG_LEIZHENYU;
    else if(code==20||code==21||code==22||code==23||code==24||code==25)
        icon = IMG_ZHONGXUE;
    else
        icon = IMG_NA;
    ui_draw_image(lcd, 166, 240, icon);//云量图标

}
