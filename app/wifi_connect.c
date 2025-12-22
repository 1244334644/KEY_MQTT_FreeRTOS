#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "board.h"
#include "espat.h"   // 引入ESP驱动
#include "wifi_connect.h"


// ================= 配置部分 =================
#define WIFI_SSID       "0D66"      // 请修改为你的WiFi名称
#define WIFI_PASS       "gnda9579"  // 请修改为你的WiFi密码


void wireless_init(void)
{
	if(!espat_init())
	{
		printf("[AT] ESPat Init Error\r\n");
		goto err;
	}
	printf("[AT] ESPat Init Success\r\n");
	if(!espat_wifi_init())
	{
		printf("[WIFI] WIFI Init Error\r\n");
		goto err;
	}
	printf("[WIFI] WIFI Init Success\r\n");

	return;

err:
	printf("[WIFI] WIFI Init Error\r\n");
}

void wireless_wait_connect(void)
{
    printf("[WIFI] Start connecting to %s...\r\n", WIFI_SSID);

    while (1)
    {
        // 1. 发送连接命令
        // 这一步如果不成功（返回 false），直接重试
        if(espat_connect_wifi(WIFI_SSID, WIFI_PASS, NULL))
        {
            printf("[WIFI] Command sent, waiting for IP...\r\n");
            
            // 2. 循环检查连接状态 (等待约 10 秒)
            for(int i = 0; i < 20; i++)
            {
                vTaskDelay(pdMS_TO_TICKS(500)); // 每 500ms 查一次
                
                esp_wifi_info_t wifi = {0};
                // 获取状态
                if(espat_get_wifi_info(&wifi))
                {
                    // 状态 2: Got IP, 状态 3: Connected, 状态 4: Disconnected
                    // 只要 connected 为 true (在 espat.c 里解析正确的话)
                    if(wifi.connected) 
                    {
                        printf("[WIFI] Connected Success!\r\n");
                        printf("[WIFI] SSID: %s, Channel: %d, RSSI: %d\r\n", 
                                wifi.ssid, wifi.channel, wifi.rssi);      
                        return; // 终于连上了，退出死循环，让程序往下跑
                    }
                }
            }
        }
        else
        {
             printf("[WIFI] Send CWJAP command failed or timeout\r\n");
        }

        // 3. 如果跑到这里，说明超时了或者连接失败了
        printf("[WIFI] Connect failed, Retrying in 2 seconds...\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 循环回到开头，再次尝试 espat_connect_wifi
    }
}
