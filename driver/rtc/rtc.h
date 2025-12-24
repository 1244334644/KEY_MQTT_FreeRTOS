#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>

typedef struct 
{
    uint16_t year;   //年份
    uint8_t month;   //月份
    uint8_t day;    //日期
    uint8_t week;    //星期
    uint8_t hour;    //小时
    uint8_t minute;  //分钟
    uint8_t second;  //秒钟
  
} rtc_date_time_t;

void rtc_init(void);
void rtc_set_time(const rtc_date_time_t *date_time);
void rtc_get_time(rtc_date_time_t *date_time);



#endif // __RTC_H__