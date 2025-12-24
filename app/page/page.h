#ifndef __PAGE_H__
#define __PAGE_H__

#include "lcd.h"
#include "rtc.h"

void welcome_display(lcd_desc_t lcd);
void error_page_display(lcd_desc_t lcd, const char *msg);
void wifi_page_display(lcd_desc_t lcd);
void main_page_display(lcd_desc_t lcd);
void main_page_redraw_wifi_ssid(lcd_desc_t lcd, char *ssid);
void main_page_redraw_inner_temp(lcd_desc_t lcd,float temp);
void main_page_redraw_inner_humi(lcd_desc_t lcd, float humi);
void main_page_redraw_outdoor_city(lcd_desc_t lcd, const char *city);
void main_page_redraw_outdoor_temp(lcd_desc_t lcd,float temp);
void main_page_redraw_outdoor_weather_icon(lcd_desc_t lcd,const int code);
void main_page_redraw_date(lcd_desc_t lcd, rtc_date_time_t *time);
void main_page_redraw_time(lcd_desc_t lcd, rtc_date_time_t *time);


#endif // __PAGE_H__