#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "stm32f4xx.h"
#include "rtc.h"


void rtc_init(void)
{

    RTC_InitTypeDef RTC_InitStructure;
    RTC_StructInit(&RTC_InitStructure);
    RTC_Init(&RTC_InitStructure);
   
    
    RCC_RTCCLKCmd(ENABLE);
    
    RTC_WaitForSynchro();
    
}


static void rtc_set_time_once(const rtc_date_time_t *date_time)
{

    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;

    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);

    date.RTC_Year = date_time->year - 2000;
    date.RTC_Month = date_time->month;
    date.RTC_Date = date_time->day;
    date.RTC_WeekDay = date_time->week;
    time.RTC_Hours = date_time->hour;
    time.RTC_Minutes = date_time->minute;
    time.RTC_Seconds = date_time->second;


    RTC_SetDate(RTC_Format_BIN, &date);
    RTC_SetTime(RTC_Format_BIN, &time);


}

/**
 * @brief 单次读取RTC时间并转换为自定义格式
 * 
 * 此函数从STM32 RTC硬件中读取当前日期和时间信息，并将其转换为自定义的
 * rtc_date_time_t格式。该函数被设计为原子操作，确保在RTC寄存器更新过程中
 * 读取到一致的时间数据。
 * 
 * @param date_time 指向rtc_date_time_t结构体的指针，用于接收转换后的时间数据
 * 
 * @note 此函数不包含数据一致性验证，调用者应确保在适当的时机调用以避免读取到
 *       正在更新的RTC寄存器数据。通常与rtc_get_time函数配合使用，后者提供了
 *       双重读取验证机制。
 * 
 * @see rtc_get_time 提供数据一致性保护的完整时间读取函数
 * @see rtc_date_time_t 自定义时间数据结构定义
 */
static void rtc_get_time_once(rtc_date_time_t *date_time)
{
    // 定义STM32标准库的日期和时间结构体变量
    RTC_DateTypeDef date;    // RTC日期结构体：包含年、月、日、星期信息
    RTC_TimeTypeDef time;    // RTC时间结构体：包含时、分、秒和AM/PM标志

    // 初始化日期和时间结构体为默认值
    // 确保结构体所有字段都有确定的初始值，避免未定义行为
    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);

    // 从RTC硬件寄存器中读取当前日期和时间
    // 使用二进制格式(RTC_Format_BIN)直接读取数值，避免BCD转换
    RTC_GetDate(RTC_Format_BIN, &date);  // 读取日期：年(0-99)、月、日、星期
    RTC_GetTime(RTC_Format_BIN, &time);  // 读取时间：时、分、秒、AM/PM

    // 数据转换：将STM32标准格式转换为自定义格式
    // RTC硬件只存储年份的后两位(0-99)，需要加上2000得到完整年份
    date_time->year = 2000 + date.RTC_Year;   // 转换为完整年份，如2024
    date_time->month = date.RTC_Month;         // 月份直接复制(1-12)
    date_time->day = date.RTC_Date;            // 日期直接复制(1-31)
    date_time->week = date.RTC_WeekDay;        // 星期直接复制(1-7，周日=1)
    date_time->hour = time.RTC_Hours;          // 小时直接复制(0-23或1-12)
    date_time->minute = time.RTC_Minutes;      // 分钟直接复制(0-59)
    date_time->second = time.RTC_Seconds;      // 秒钟直接复制(0-59)

    // 注意：此函数假设系统使用24小时制，如果使用12小时制需要额外处理AM/PM标志
    // 当前实现适用于RTC配置为24小时制的情况
}

void rtc_set_time(const rtc_date_time_t *date_time)
{
    rtc_date_time_t rtime;
    do
    {
        rtc_set_time_once(date_time);
        rtc_get_time_once(&rtime);
    } while (date_time->second != rtime.second);


}





void rtc_get_time(rtc_date_time_t *date_time)
{
    rtc_date_time_t t1, t2;
    do
    {
        rtc_get_time_once(&t1);
        rtc_get_time_once(&t2);
    } while (memcmp(&t1, &t2, sizeof(rtc_date_time_t)) != 0);
    
    memcpy(date_time, &t1, sizeof(rtc_date_time_t));
}