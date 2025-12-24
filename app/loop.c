#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "work_queue.h"
#include "board.h"
#include "espat.h"
#include "key.h"
#include "aht20.h"
#include "led.h"
#include "wifi_connect.h"
#include "onenet.h"
#include "loop.h"
#include "weather.h"
#include "page.h"
#include "rtc.h"

#define MILLISECONDS(x) (x)
#define SECONDS(x)  		MILLISECONDS((x) * 1000)
#define MINUTES(x)  		SECONDS((x) * 60)
#define HOURS(x)    		MINUTES((x) * 60)
#define DAYS(x)    		HOURS((x) * 24)


#define TIME_SYNC_INTERVAL   HOURS(1)
#define TIME_UPDATE_INTERVAL   SECONDS(1)
#define WIFI_UPDATE_INTERVAL   SECONDS(5)
#define MQTT_UPDATE_INTERVAL   MILLISECONDS(200)  // 【关键优化】从1秒改为200ms，快速响应
#define KEY_UPDATE_INTERVAL   MILLISECONDS(50)
#define INNER_UPDATE_INTERVAL   SECONDS(3)
#define OUTDOOR_UPDATE_INTERVAL   MINUTES(1) 
// ================= 全局变量 =================
// 这些变量在 onenet.c 中被 extern 引用，用于模拟传感器数据
bool led1_state, led2_state, key1_state, key2_state;

// 【新增】状态改变标志
extern bool state_changed;

// 【新增】声明外部全局变量（在onenet.c中定义）
extern float g_temperature;
extern float g_inner_temp;
extern float g_inner_humi;
extern char g_city[32];
extern char g_location[128];
extern int weather_code;


static TimerHandle_t wifi_update_timer,mqtt_update_timer,key_update_timer,inner_update_timer,outdoor_update_timer,time_sync_timer,time_update_timer;



static void time_sync(void)
{

	uint32_t restart_sync_delay = TIME_SYNC_INTERVAL;
	rtc_date_time_t rtc_date={0};
	esp_time_t esp_date={0};  // 【修复警告】提前声明，避免goto跳过初始化
	
	/**
	 * 【优化】先检查RTC时间是否有效
	 * 
	 * 如果RTC已经有有效时间（年份>=2020），说明之前已经同步过，
	 * 就不需要频繁查询SNTP，直接使用RTC时间即可。
	 * 只有在首次启动或RTC时间无效时才查询SNTP。
	 */
	rtc_get_time(&rtc_date);
	if(rtc_date.year >= 2020)
	{
		printf("[TIME] RTC time valid: %04d-%02d-%02d %02d:%02d:%02d, skip SNTP query\r\n", 
			rtc_date.year, rtc_date.month, rtc_date.day, 
			rtc_date.hour, rtc_date.minute, rtc_date.second);
		
		// RTC时间有效，每24小时才同步一次SNTP（保持长期准确）
		restart_sync_delay = HOURS(24);
		goto err;
	}
	
	printf("[TIME] RTC time invalid (%d), querying SNTP...\r\n", rtc_date.year);
	
	/**
	 * 获取SNTP网络时间
	 * 
	 * 通过ESP-AT模块向SNTP服务器请求当前网络时间，
	 * 如果获取失败（返回false），则记录错误并设置5秒后重试。
	 */
	if(!espat_sntp_get_time(&esp_date))
	{
		printf("[SNTP] Get Time Error\r\n");
		restart_sync_delay = SECONDS(5);  // 5秒后重试（避免频繁查询）
		goto err;
	}
	
	/**
	 * 验证时间数据有效性
	 * 
	 * 检查获取的网络时间年份是否有效（大于等于2020），
	 * 如果年份小于2020，说明SNTP还没同步成功，设置5秒后重试。
	 */
	if(esp_date.year <2020)
	{
		printf("[SNTP] Waiting for time sync... (got %d, retrying in 5s)\r\n", esp_date.year);
		restart_sync_delay = SECONDS(5);  // 5秒后重试
		goto err;
	}

	/**
	 * 打印时间同步成功信息
	 * 
	 * 显示同步成功的网络时间信息，格式：YYYY-MM-DD HH:MM:SS (星期)
	 * 便于调试和监控时间同步状态。
	 */
	printf("[SNTP] Time Sync Success: %04d-%02d-%02d %02d:%02d:%02d (%d)\r\n", esp_date.year, esp_date.month, 
		esp_date.day, esp_date.hour, esp_date.minute, esp_date.second, esp_date.weekday);
	
	/**
	 * 设置RTC实时时钟
	 * 
	 * 将网络时间转换为RTC时间格式，并写入硬件RTC芯片，
	 * 确保系统断电后仍能保持准确时间。
	 */
	
	rtc_date.year = esp_date.year;
	rtc_date.month = esp_date.month;
	rtc_date.day = esp_date.day;
	rtc_date.hour = esp_date.hour;
	rtc_date.minute = esp_date.minute;
	rtc_date.second = esp_date.second;
	rtc_date.week = esp_date.weekday;
	rtc_set_time(&rtc_date);
	
	// SNTP同步成功后，24小时才同步一次
	restart_sync_delay = HOURS(24);

	/**
	 * 立即触发时间显示更新
	 * 
	 * 设置time_update_delay为0，确保时间显示任务立即执行，
	 * 将新同步的时间显示在LCD屏幕上。
	 * 
	 * 注意：这里设置为0而不是10，确保时间更新立即执行，
	 * 避免用户看到旧的时间信息。
	 */
err:
	xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(restart_sync_delay),0);
}

/**
 * @brief WiFi状态更新任务函数
 * 
 * 该函数负责监控ESP-AT模块的WiFi连接状态变化，
 * 当检测到连接状态变化时，更新LCD屏幕上的WiFi状态显示。
 * 
 * 执行流程：
 * 1. 检查延时计数器，未到执行时间则直接返回
 * 2. 重置延时计数器为5秒间隔（WIFI_UPDATE_INTERVAL）
 * 3. 获取当前WiFi连接信息
 * 4. 比较新旧WiFi信息，仅当连接状态变化时才更新显示
 * 5. 根据连接状态更新LCD显示和打印日志
 * 
 * 优化策略：
 * - 状态变化检测：仅当WiFi连接状态发生变化时才更新显示
 * - 数据完整性检查：确保获取的WiFi信息有效
 * - 避免频繁更新：5秒间隔减少不必要的屏幕刷新
 * 
 * @param lcd LCD描述符，用于更新屏幕显示
 * 
 * @note 该函数通过main_loop()周期性调用
 * @warning 需要确保ESP-AT模块已正确初始化
 * 
 * @see main_loop() 主循环调度函数
 * @see espat_get_wifi_info() WiFi信息获取函数
 * @see main_page_redraw_wifi_ssid() WiFi状态显示更新函数
 * 
 * 执行间隔：5秒（WIFI_UPDATE_INTERVAL = SECONDS(5)）
 */
static void wifi_update()
{
	/**
	 * 静态变量：保存上一次的WiFi连接信息
	 * 
	 * 使用静态变量记录上一次的WiFi状态，用于比较状态变化，
	 * 避免每次函数调用时重新分配内存，提高执行效率。
	 */
	static esp_wifi_info_t last_info={0};
	
	/**
	 * 重置WiFi状态更新延时计数器
	 * 
	 * 将延时计数器设置为5秒间隔（WIFI_UPDATE_INTERVAL），
	 * 确保下次WiFi状态检查将在5秒后执行。
	 */
	xTimerChangePeriod(wifi_update_timer, pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL),0);

	/**
	 * 获取当前WiFi连接信息
	 * 
	 * 通过ESP-AT模块获取当前的WiFi连接状态和详细信息，
	 * 如果获取失败（返回false），则记录错误并直接返回。
	 */
	esp_wifi_info_t info={0};
	if(!espat_get_wifi_info(&info))
	{
		printf("[WIFI] Get Info Error\r\n");
		return;
	}
	
	/**
	 * 检查WiFi信息是否发生变化
	 * 
	 * 使用memcmp比较新旧WiFi信息的完整结构体，
	 * 如果完全相同（返回0），说明信息未变化，直接返回。
	 * 
	 * 这种比较方式可以检测到SSID、BSSID、信号强度等任何字段的变化。
	 */
	if(memcmp(&info, &last_info, sizeof(esp_wifi_info_t)) == 0)
	{
		return;  // WiFi信息未变化，无需更新显示
	}
	
	/**
	 * 检查连接状态是否发生变化
	 * 
	 * 专门检查连接状态字段是否发生变化，
	 * 如果连接状态相同，即使其他信息变化也不更新显示。
	 * 
	 * 这种设计确保只有在连接/断开状态变化时才更新屏幕。
	 */
	if(last_info.connected == info.connected)
	{
		return;  // 连接状态未变化，无需更新显示
	}
	
	/**
	 * 处理WiFi连接成功的情况
	 * 
	 * 当检测到WiFi从断开变为连接状态时：
	 * 1. 打印连接成功日志
	 * 2. 显示详细的WiFi信息（SSID、BSSID、信道、信号强度）
	 * 3. 更新LCD屏幕显示连接的SSID
	 */
	if(info.connected)
	{
		printf("[WIFI] Connected\r\n");
		printf("[WIFI] SSID: %s, BSSID: %02x:%02x:%02x:%02x:%02x:%02x, Channel: %d, RSSI: %d\r\n", 
			info.ssid, info.bssid[0], info.bssid[1], info.bssid[2], info.bssid[3], info.bssid[4], info.bssid[5], info.channel, info.rssi);
	}
	/**
	 * 处理WiFi断开连接的情况
	 * 
	 * 当检测到WiFi从连接变为断开状态时：
	 * 1. 打印断开连接日志
	 * 2. 更新LCD屏幕显示"wifi_lost"提示信息
	 */
	else
	{
		printf("[WIFI] Disconnected\r\n");
	}
	
	/**
	 * 保存当前WiFi信息作为下一次比较的基准
	 * 
	 * 使用memcpy将当前WiFi信息复制到last_info静态变量中，
	 * 确保下一次函数调用时能够正确比较状态变化。
	 */
	memcpy(&last_info, &info, sizeof(esp_wifi_info_t));
	
}

static void time_update()
{
	static rtc_date_time_t last_date={0};
	xTimerChangePeriod(time_update_timer, pdMS_TO_TICKS(TIME_UPDATE_INTERVAL),0);

	rtc_date_time_t date={0};
	rtc_get_time(&date);
	
	if(memcmp(&last_date, &date, sizeof(rtc_date_time_t)) == 0)
	{
		return;
	}

	memcpy(&last_date, &date, sizeof(rtc_date_time_t));
	main_page_redraw_time(lcd,&date);
	main_page_redraw_date(lcd,&date);
	
}

static void mqtt_update()
{
    static bool mqtt_connected = false;
    static uint32_t last_send_time = 0;
    static uint32_t last_connect_time = 0;
    static uint32_t send_fail_count = 0;  // 【新增】发送失败计数
    static const uint32_t connect_retry_interval = 5000;  // 连接重试间隔5秒
    
    unsigned char *dataPtr = NULL;
    uint32_t current_time = xTaskGetTickCount();
    
    // 【新增】如果连续发送失败超过3次，标记为断开连接
    if(send_fail_count >= 3)
    {
        printf("[MQTT] Too many send failures (%d), marking as disconnected\n", send_fail_count);
        mqtt_connected = false;
        send_fail_count = 0;
        return;
    }
    
    // 1. 检查MQTT连接状态（避免频繁连接）
    if(!mqtt_connected)
    {
        // 限制连接尝试频率
        if((current_time - last_connect_time) >= pdMS_TO_TICKS(connect_retry_interval))
        {
            if(OneNet_DevLink() == 0)
            {
                printf("OneNet Connected.\n");
                mqtt_connected = true;
                send_fail_count = 0;  // 【重要】连接成功后清零失败计数
                OneNET_Subscribe();
                last_connect_time = current_time;
            }
            else
            {
                printf("OneNet Connect Failed, retry in %ds\n", connect_retry_interval/1000);
                last_connect_time = current_time;
            }
        }
        return;  // 未连接时直接返回
    }
    
    // 2. 【关键优化】循环检查接收数据，处理所有待处理消息（避免消息积压）
    uint8_t msg_count = 0;
    while((dataPtr = ESP32_GetIPD(10)) != NULL && msg_count < 10)  // 【优化】最多处理10条消息，超时10ms
    {
        msg_count++;
        printf("[MQTT] Msg#%d from cloud\n", msg_count);
        OneNet_RevPro(dataPtr);
        vTaskDelay(pdMS_TO_TICKS(5));  // 短暂延时5ms
    }
    
    // 如果收到了云端消息，立即上报当前状态（单次上报，快速响应）
    if(msg_count > 0)
    {
        printf("[MQTT] Processed %d msgs, report now\n", msg_count);
        
        // 【关键优化】确保距离上次发送至少300ms，避免ESP32发送失败
        uint32_t time_since_last = xTaskGetTickCount() - last_send_time;
        if(time_since_last < pdMS_TO_TICKS(300))
        {
            uint32_t wait_time = 300 - (time_since_last * portTICK_PERIOD_MS);
            printf("[MQTT] Wait %dms before send (avoid ESP32 overload)\n", wait_time);
            vTaskDelay(pdMS_TO_TICKS(wait_time));
        }
        
        // 立即上报1次
        OneNet_SendData();
        last_send_time = xTaskGetTickCount();
        state_changed = false;
        
        // 【重要】不要延迟，立即返回，让下一个200ms循环再次检查是否有新消息
        return;
    }
    
    // 3. 【关键优化】检查本地状态改变标志（按键操作），立即上报
    if(state_changed)
    {
        // 【优化】确保至少300ms间隔，避免ESP32发送失败
        if((current_time - last_send_time) >= pdMS_TO_TICKS(300))
        {
            printf("[MQTT] Local state changed, report\n");
            OneNet_SendData();
            state_changed = false;
            last_send_time = xTaskGetTickCount();
        }
        // else: 保持标志，等待下次200ms循环
    }
    
    // 4. 定期上报 - 改为5秒一次（降低频率，避免干扰实时上报）
    else if((current_time - last_send_time) >= pdMS_TO_TICKS(5000))
    {
        printf("[MQTT] Periodic report (LED1=%d, LED2=%d, InnerT=%.1f, OuterT=%.1f)\n", 
            led1_state, led2_state, g_inner_temp, g_temperature);
        OneNet_SendData();
        last_send_time = xTaskGetTickCount();
    }
    
    // 【关键】移除所有vTaskDelay，让函数快速返回，避免阻塞工作队列
    
	

}

static void key_update()
{
    static bool key1_pressed = false;
    static bool key2_pressed = false;
    static uint32_t debug_counter = 0;
    
    // 【调试】每1000次打印一次,确认函数在运行 (50ms * 1000 = 50秒一次)
    debug_counter++;
    if(debug_counter >= 1000)
    {
        printf("[KEY] key_update running, key1=%d, key2=%d\n", key_read(key1), key_read(key2));
        debug_counter = 0;
    }

    // 检测 Key1
    uint8_t key1_val = key_read(key1);
    if(key1_val == 0)  // 按键按下(低电平)
    {
        if(!key1_pressed)
        {
            uint32_t press_time = xTaskGetTickCount();  // 记录按键时刻
            key1_pressed = true;
            key1_state = 1;
            led_toggle(led1);
            led1_state = !led1_state;
            state_changed = true;  // 【关键】设置状态改变标志
            uint32_t toggle_time = xTaskGetTickCount() - press_time;  // 计算LED响应时间
            printf("[KEY] Key1 pressed, LED1=%d, response_time=%dms, state_changed=true\n", 
                led1_state, toggle_time);
        }
    }
    else
    {
        if(key1_pressed)
        {
            key1_pressed = false;
            key1_state = 0;
            printf("[KEY] Key1 Released\n");
        }
    }

    // 检测 Key2
    uint8_t key2_val = key_read(key2);
    if(key2_val == 0)  // 按键按下(低电平)
    {
        if(!key2_pressed)
        {
            key2_pressed = true;
            key2_state = 1;
            led_toggle(led2);
            led2_state = !led2_state;
            state_changed = true;  // 【关键】设置状态改变标志
            printf("Key2 pressed, LED2=%d, state_changed=true\n", led2_state);
        }
    }
    else
    {
        if(key2_pressed)
        {
            key2_pressed = false;
            key2_state = 0;
            printf("Key2 Released\n");
        }
    }
}


static void inner_update()
{
	
	static float last_temperature = 0.0f;
	static float last_humidity = 0.0f;

	xTimerChangePeriod(inner_update_timer, pdMS_TO_TICKS(INNER_UPDATE_INTERVAL),0);

	if(!aht20_start_measure(aht20))
	{
		printf("[AHT20] Measure Error\r\n");
		return;
	}
	if(!aht20_wait_for_measure(aht20))
	{
		printf("[AHT20] Wait Error\r\n");
		return;
	}
	float temp=0.0f, humi=0.0f;
	if(!aht20_read_measurement(aht20,&temp, &humi))
	{
		printf("[AHT20] Read Error\r\n");		
		return;
	}
	if(temp==last_temperature && humi==last_humidity)
	{
		return;
	}
	last_temperature = temp;
	last_humidity = humi;
	
	// 【关键修复】更新全局变量，供OneNet_FillBuf使用
	g_inner_temp = temp;
	g_inner_humi = humi;
	
	printf("[AHT20] Temperature: %.1f, Humidity: %.1f\r\n", temp, humi);
	printf("[UPDATE] Inner: %.1f°C %.1f%%, Outer: %.1f°C, Weather: %d\r\n", 
		g_inner_temp, g_inner_humi, g_temperature, weather_code);
    main_page_redraw_inner_temp(lcd,temp);
	main_page_redraw_inner_humi(lcd,humi);
}

static void outdoor_update()
{
	static weather_info_t last_weather={0};

	xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL),0);

	weather_info_t weather={0};
	const char *weather_url = "https://api.seniverse.com/v3/weather/now.json?key=SAQyoOxFBGFh4lfRp&location=Guangzhou&language=en&unit=c";
	const char *weather_http_response = espat_http_get(weather_url, NULL);
	if(weather_http_response == NULL)
	{
		printf("[WEATHER] HTTP GET Error\r\n");
		return;
	}
	if(!parse_seniverse_response(weather_http_response, &weather))
	{
		printf("[WEATHER] Parse WEATHER Response Error\r\n");
		return;
	}
	if(memcmp(&weather, &last_weather, sizeof(weather_info_t)) == 0)
	{
		return;
	}
	memcpy(&last_weather, &weather, sizeof(weather_info_t));
	
	// 【关键修复】更新全局变量，供OneNet_FillBuf使用
	strncpy(g_city, weather.city, sizeof(g_city) - 1);
	g_city[sizeof(g_city) - 1] = '\0';  // 确保字符串结束
	
	// 【优化】提取location的最后一部分（如"panyu"），减少字符串长度
	// 原始格式：Guangzhou,Guangzhou,Guangdong,China -> 提取最后一个逗号后的部分
	const char *last_part = strrchr(weather.loaction, ',');
	if(last_part != NULL)
	{
		last_part++;  // 跳过逗号
		// 如果最后一部分太长（如"China"），则使用倒数第二部分（如"Guangdong"）
		if(strlen(last_part) > 10)  // "China"这种国家名通常较短，我们想要更具体的区域
		{
			// 查找倒数第二个逗号
			char temp[128];
			strncpy(temp, weather.loaction, sizeof(temp) - 1);
			temp[sizeof(temp) - 1] = '\0';
			
			char *second_last = strrchr(temp, ',');
			if(second_last != NULL)
			{
				*second_last = '\0';  // 截断最后一部分
				last_part = strrchr(temp, ',');
				if(last_part != NULL)
				{
					last_part++;
					strncpy(g_location, last_part, sizeof(g_location) - 1);
				}
				else
				{
					strncpy(g_location, temp, sizeof(g_location) - 1);
				}
			}
			else
			{
				strncpy(g_location, weather.loaction, sizeof(g_location) - 1);
			}
		}
		else
		{
			strncpy(g_location, last_part, sizeof(g_location) - 1);
		}
	}
	else
	{
		// 没有逗号，直接使用完整location
		strncpy(g_location, weather.loaction, sizeof(g_location) - 1);
	}
	g_location[sizeof(g_location) - 1] = '\0';
	
	g_temperature = weather.temperature;
	
	// 解析天气代码 (简单映射)
	if(strstr(weather.weather, "Sunny") || strstr(weather.weather, "Clear"))
		weather_code = 0;  // 晴天
	else if(strstr(weather.weather, "Cloudy"))
		weather_code = 1;  // 多云
	else if(strstr(weather.weather, "Rain") || strstr(weather.weather, "Shower"))
		weather_code = 2;  // 雨天
	else if(strstr(weather.weather, "Snow"))
		weather_code = 3;  // 雪天
	else
		weather_code = 1;  // 默认多云
	
	printf("[WEATHER] %s %s %.1f°C (code=%d)\n", 
		g_city, weather.weather, g_temperature, weather_code);
        
	main_page_redraw_outdoor_temp(lcd, weather.temperature);
	main_page_redraw_outdoor_weather_icon(lcd, weather.weather_code);
}

typedef void (*app_job_t)(void);

static void app_work(void *param)
{
	app_job_t job=(app_job_t)param;
	job();
}

static void mloop_timer_cb(TimerHandle_t timer)
{
	app_job_t job=(app_job_t)pvTimerGetTimerID(timer);
	
	// 【关键优化】只将最快的任务直接执行，避免栈溢出
	// key_update: ~1ms（仅GPIO读取和LED切换，栈消耗很小）
	if(job == key_update)
	{
		job();  // 直接执行，延迟 < 1ms，避免队列阻塞
	}
	else
	{
		// 其他任务（mqtt_update, wifi_update, inner_update, outdoor_update）放入队列
		// mqtt_update虽然需要快速响应，但函数较复杂，可能导致栈溢出
		workqueue_run(app_work, job);
	}
	
}

static void timer_update_cb(TimerHandle_t timer)
{
	app_job_t job=(app_job_t)pvTimerGetTimerID(timer);
	job();
	
}

void main_loop_init(void)
{	
	printf("[LOOP] Creating timers...\n");

	time_update_timer = xTimerCreate("time_update", pdMS_TO_TICKS(TIME_UPDATE_INTERVAL), pdTRUE,time_update, timer_update_cb);
	time_sync_timer = xTimerCreate("time_sync", pdMS_TO_TICKS(200), pdFALSE,time_sync, mloop_timer_cb);
	wifi_update_timer = xTimerCreate("wifi_update", pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), pdTRUE, wifi_update, mloop_timer_cb);
	mqtt_update_timer = xTimerCreate("mqtt_update", pdMS_TO_TICKS(MQTT_UPDATE_INTERVAL), pdTRUE, mqtt_update, mloop_timer_cb);
	key_update_timer = xTimerCreate("key_update", pdMS_TO_TICKS(KEY_UPDATE_INTERVAL), pdTRUE, key_update, mloop_timer_cb);
	inner_update_timer = xTimerCreate("inner_update", pdMS_TO_TICKS(INNER_UPDATE_INTERVAL), pdTRUE,inner_update, mloop_timer_cb);
    outdoor_update_timer = xTimerCreate("outdoor_update", pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), pdTRUE,outdoor_update, mloop_timer_cb);

	if(wifi_update_timer == NULL || mqtt_update_timer == NULL || key_update_timer == NULL || inner_update_timer == NULL || outdoor_update_timer == NULL)
	{
		printf("[LOOP] Timer creation failed!\n");
		return;
	}
	printf("[LOOP] Timers created successfully\n");
	
	printf("[LOOP] Running initial tasks...\n");

    workqueue_run(app_work,time_sync);
	workqueue_run(app_work, wifi_update);
	workqueue_run(app_work, mqtt_update);
	workqueue_run(app_work, key_update);
    workqueue_run(app_work,inner_update);
    workqueue_run(app_work,outdoor_update);

	printf("[LOOP] Starting timers...\n");

    xTimerStart(time_update_timer, 0);
	xTimerStart(time_sync_timer, 0);
	xTimerStart(wifi_update_timer, 0);
	xTimerStart(mqtt_update_timer, 0);
	xTimerStart(key_update_timer, 0);
	xTimerStart(inner_update_timer, 0);
    xTimerStart(outdoor_update_timer, 0);

    
	printf("[LOOP] Initialization complete. Key update interval: %dms\n", KEY_UPDATE_INTERVAL);
}
