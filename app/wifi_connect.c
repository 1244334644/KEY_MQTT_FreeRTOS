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
#include "ble_prov.h"  // BLE 配网

// 配网模式
typedef enum {
    PROV_MODE_AUTO,     // 自动检测：优先W25Q16，无配置则进入BLE配网
    PROV_MODE_BLE,      // 强制BLE配网
    PROV_MODE_DIRECT    // 直接连接（使用硬编码或已保存配置）
} prov_mode_t;

// 配置
#define PROV_TIMEOUT_MS   120000  // BLE 配网超时时间 (2分钟)


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

/**
 * @brief 尝试连接 WiFi
 * @param ssid WiFi 名称
 * @param password WiFi 密码
 * @return true 连接成功，false 连接失败
 */
static bool try_connect_wifi(const char *ssid, const char *password)
{
    printf("[WIFI] Connecting to %s...\r\n", ssid);
    
    for (int retry = 0; retry < 3; retry++)
    {
        if (espat_connect_wifi(ssid, password, NULL))
        {
            // 等待连接成功 (最多 10 秒)
            for (int i = 0; i < 20; i++)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                
                esp_wifi_info_t wifi = {0};
                if (espat_get_wifi_info(&wifi) && wifi.connected)
                {
                    printf("[WIFI] Connected! SSID: %s, RSSI: %d\r\n", 
                           wifi.ssid, wifi.rssi);
                    return true;
                }
            }
        }
        
        printf("[WIFI] Connect failed, retry %d/3\r\n", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return false;
}

/**
 * @brief 智能 WiFi 连接 (支持 BLE 配网)
 * 
 * 工作流程：
 * 1. 检查 W25Q16 是否有保存的配置
 * 2. 有配置 → 直接连接
 * 3. 无配置 → 启动 BLE 配网，等待手机发送配置
 * 4. 配网成功 → 保存配置并连接
 */
void wireless_wait_connect(void)
{
    wifi_config_t config = {0};
    
    // 1. 尝试从 W25Q16 加载配置
    if (wifi_config_load(&config))
    {
        printf("[WIFI] Found saved config: %s\r\n", config.ssid);
        
        if (try_connect_wifi(config.ssid, config.password))
        {
            return; // 连接成功
        }
        
        // 连接失败，可能是密码变了，清除旧配置
        printf("[WIFI] Saved config failed, starting BLE provision...\r\n");
        wifi_config_clear();
    }
    
    // 2. 无有效配置，启动 BLE 配网
    printf("[PROV] Starting BLE provision...\r\n");
    
    if (!ble_prov_init())
    {
        printf("[PROV] BLE init failed!\r\n");
        return;
    }
    
    if (!ble_prov_start())
    {
        printf("[PROV] BLE start failed!\r\n");
        return;
    }
    
    printf("[PROV] Waiting for BLE config (timeout: %d ms)...\r\n", PROV_TIMEOUT_MS);
    printf("[PROV] Please open WeChat mini program to send WiFi config\r\n");
    
    // 3. 等待配网数据
    if (ble_prov_wait_for_config(&config, PROV_TIMEOUT_MS))
    {
        printf("[PROV] Got WiFi config: SSID=%s\r\n", config.ssid);
        
        // 4. 停止 BLE 广播
        ble_prov_stop();
        
        // 5. 保存配置到 W25Q16
        wifi_config_save(&config);
        
        // 6. 连接 WiFi
        if (try_connect_wifi(config.ssid, config.password))
        {
            printf("[PROV] Provisioning success!\r\n");
        }
        else
        {
            printf("[PROV] WiFi connect failed after provisioning\r\n");
        }
    }
    else
    {
        printf("[PROV] BLE provision timeout\r\n");
        ble_prov_stop();
    }
}
