/**
 * @file ble_prov.c
 * @brief ESP32-C3 BLE 配网实现 (AT指令方式)
 * 
 * 工作流程：
 * 1. STM32 发送 AT 指令初始化 ESP32-C3 的 BLE
 * 2. ESP32-C3 开启 BLE 广播，等待手机连接
 * 3. 手机通过 BLE 发送 WiFi 账号密码
 * 4. ESP32-C3 通过 UART 通知 STM32
 * 5. STM32 保存配置到 W25Q16 并连接 WiFi
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ble_prov.h"
#include "espat.h"
#include "w25q16.h"
#include "w25q16_desc.h"
#include "FreeRTOS.h"
#include "task.h"

// 外部 W25Q16 描述符
extern w25q16_desc_t w25q16;

// BLE 服务和特征 UUID (自定义)
#define BLE_SVC_UUID        "FFE0"  // 服务 UUID
#define BLE_CHAR_UUID       "FFE1"  // 特征 UUID (用于发送 WiFi 配置)

// BLE 配置参数
#define BLE_DEVICE_NAME     "SmartDevice_001"  // BLE 设备名称
#define BLE_MAX_CONN        1                   // 最大连接数

// 内部状态
static ble_prov_state_t g_ble_state = BLE_PROV_IDLE;
static wifi_config_t g_wifi_config = {0};

// 前向声明
static bool ble_send_at_cmd(const char *cmd, uint32_t timeout);
static bool ble_parse_wifi_config(const char *data, wifi_config_t *config);

/**
 * @brief 发送 AT 指令并等待响应
 */
static bool ble_send_at_cmd(const char *cmd, uint32_t timeout)
{
    return espat_write_command(cmd, timeout);
}

/**
 * @brief 测试 ESP32-C3 BLE 功能是否可用
 */
bool ble_prov_test(void)
{
    printf("\n========== BLE Test Start ==========\n");
    
    // 1. 测试 AT 指令
    printf("[BLE] Step 1: Test AT...\n");
    if (!ble_send_at_cmd("AT\r\n", 1000))
    {
        printf("[BLE] ERROR: AT test failed, ESP32 not responding!\n");
        return false;
    }
    printf("[BLE] Step 1: AT OK\n");
    
    // 2. 查询 BLE 初始化状态
    printf("[BLE] Step 2: Query BLE init status...\n");
    if (ble_send_at_cmd("AT+BLEINIT?\r\n", 1000))
    {
        extern const char *espat_get_response(void);
        const char *resp = espat_get_response();
        printf("[BLE] BLEINIT response: %s\n", resp);
        
        // 检查是否已初始化
        if (strstr(resp, "+BLEINIT:0") != NULL)
        {
            printf("[BLE] BLE is not initialized (0=disable), can be enabled\n");
        }
        else if (strstr(resp, "+BLEINIT:1") != NULL)
        {
            printf("[BLE] BLE is in client mode\n");
        }
        else if (strstr(resp, "+BLEINIT:2") != NULL)
        {
            printf("[BLE] BLE is in server mode\n");
        }
        else if (strstr(resp, "ERROR") != NULL)
        {
            printf("[BLE] ERROR: BLE not supported by this firmware!\n");
            printf("[BLE] You need to flash BLE-enabled AT firmware\n");
            return false;
        }
    }
    else
    {
        printf("[BLE] Step 2: No response, trying to init BLE...\n");
    }
    
    // 3. 尝试初始化 BLE
    printf("[BLE] Step 3: Initialize BLE (server mode)...\n");
    if (!ble_send_at_cmd("AT+BLEINIT=2\r\n", 2000))
    {
        printf("[BLE] ERROR: BLE Init failed!\n");
        printf("[BLE] Possible reasons:\n");
        printf("[BLE]   1. Firmware does not support BLE\n");
        printf("[BLE]   2. BLE already initialized\n");
        printf("[BLE]   3. WiFi is connected (BLE/WiFi share antenna)\n");
        return false;
    }
    printf("[BLE] Step 3: BLE Init OK!\n");
    
    // 4. 设置设备名称
    printf("[BLE] Step 4: Set device name...\n");
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+BLENAME=\"%s\"\r\n", BLE_DEVICE_NAME);
    if (ble_send_at_cmd(cmd, 1000))
    {
        printf("[BLE] Step 4: Device name set to '%s'\n", BLE_DEVICE_NAME);
    }
    
    // 5. 查询 BLE 地址
    printf("[BLE] Step 5: Query BLE address...\n");
    if (ble_send_at_cmd("AT+BLEADDR?\r\n", 1000))
    {
        extern const char *espat_get_response(void);
        const char *resp = espat_get_response();
        printf("[BLE] BLE Address: %s\n", resp);
    }
    
    // 6. 开始广播测试
    printf("[BLE] Step 6: Start advertising...\n");
    if (!ble_send_at_cmd("AT+BLEADVSTART\r\n", 1000))
    {
        printf("[BLE] ERROR: Start advertising failed!\n");
        return false;
    }
    printf("[BLE] Step 6: Advertising started!\n");
    
    printf("\n========== BLE Test PASSED ==========\n");
    printf("[BLE] Device name: %s\n", BLE_DEVICE_NAME);
    printf("[BLE] Please scan with your phone now!\n");
    printf("======================================\n\n");
    
    // 停止广播 (让 ble_prov_start 重新开始)
    ble_send_at_cmd("AT+BLEADVSTOP\r\n", 1000);
    
    return true;
}

/**
 * @brief 发送 AT 命令数据（用于交互式命令）
 */
static void ble_send_data(const char *data)
{
    espat_usart_write_data(data, strlen(data));
}

/**
 * @brief 发送交互式 AT 命令（等待 > 提示符后发送数据）
 * @param cmd AT 命令
 * @param data 要发送的数据
 * @param timeout 超时时间
 * @return true 成功，false 失败
 */
static bool ble_send_interactive_cmd(const char *cmd, const char *data, uint32_t timeout)
{
    // 使用 espat_write_command 发送命令
    // 如果命令需要数据输入，espat_write_command 会等待 > 并返回 true
    if (espat_write_command(cmd, timeout))
    {
        // 如果有数据需要发送
        if (data != NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(100));  // 短暂延时
            ble_send_data(data);
            printf("[BLE] Data sent: %s\n", data);
            
            // 等待最终 OK
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        return true;
    }
    return false;
}

/**
 * @brief 初始化 BLE 配网模块
 */
bool ble_prov_init(void)
{
    printf("[BLE] Initializing BLE provision...\n");
    
    // 1. 先去初始化 BLE（确保干净状态）
    ble_send_at_cmd("AT+BLEINIT=0\r\n", 1000);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 2. 初始化 BLE (设置为 Server 模式)
    printf("[BLE] Step 1: Initialize BLE as server...\n");
    if (!ble_send_at_cmd("AT+BLEINIT=2\r\n", 3000))
    {
        printf("[BLE] BLE Init failed\n");
        return false;
    }
    printf("[BLE] BLE Init success\n");
    
    // 3. 创建 GATT 服务（交互式命令）
    // AT+BLEGATTSSRVCRE=1 然后等待 > 输入服务 UUID
    printf("[BLE] Step 2: Create GATT service (interactive)...\n");
    // 服务 UUID: 0xFFE0 (16-bit)
    if (!ble_send_interactive_cmd("AT+BLEGATTSSRVCRE=1\r\n", "FFE0\r\n", 2000))
    {
        printf("[BLE] Create service failed\n");
    }
    else
    {
        printf("[BLE] Service created\n");
    }
    
    // 4. 添加特征值（交互式命令）
    printf("[BLE] Step 3: Add characteristic (interactive)...\n");
    // 参数: 服务索引=1, 特征索引=1, 属性=0x08(可写), 权限=0x01, 最大长度=64
    // UUID: 0xFFE1
    if (!ble_send_interactive_cmd("AT+BLEGATTSCHARADD=1,1,0x08,0x01,64\r\n", "FFE1\r\n", 2000))
    {
        printf("[BLE] Add characteristic failed\n");
    }
    else
    {
        printf("[BLE] Characteristic added\n");
    }
    
    // 5. 启动服务
    printf("[BLE] Step 4: Start service...\n");
    ble_send_at_cmd("AT+BLEGATTSSRVSTART=1\r\n", 2000);
    
    // 6. 查询服务状态
    printf("[BLE] Step 5: Query service status...\n");
    ble_send_at_cmd("AT+BLEGATTSSRV?\r\n", 2000);
    
    printf("[BLE] Step 6: Query characteristic status...\n");
    ble_send_at_cmd("AT+BLEGATTSCHAR?\r\n", 2000);
    
    // 7. 设置设备名称
    printf("[BLE] Step 7: Set device name...\n");
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+BLENAME=\"%s\"\r\n", BLE_DEVICE_NAME);
    ble_send_at_cmd(cmd, 1000);
    
    // 8. 设置广播参数
    printf("[BLE] Step 8: Set advertising params...\n");
    ble_send_at_cmd("AT+BLEADVPARAM=100,500,0,0,7,0,0,0,0\r\n", 1000);
    
    // 9. 设置广播数据
    printf("[BLE] Step 9: Set advertising data...\n");
    ble_send_at_cmd("AT+BLEADVDATA=\"0201060A09536D6172744465766963655F303031\"\r\n", 1000);
    
    // 10. 设置扫描响应数据
    printf("[BLE] Step 10: Set scan response data...\n");
    ble_send_at_cmd("AT+BLESCANRSPDATA=\"0A09536D617274446576696365\"\r\n", 1000);
    
    g_ble_state = BLE_PROV_IDLE;
    printf("[BLE] BLE Prov Init OK\n");
    return true;
}

/**
 * @brief 开始 BLE 广播，等待配网
 */
bool ble_prov_start(void)
{
    // 1. 开始广播
    if (!ble_send_at_cmd("AT+BLEADVSTART\r\n", 1000))
    {
        printf("[BLE] Start advertising failed\n");
        return false;
    }
    
    g_ble_state = BLE_PROV_WAITING;
    printf("[BLE] Advertising started, waiting for connection...\n");
    return true;
}

/**
 * @brief 停止 BLE 广播
 */
bool ble_prov_stop(void)
{
    // 断开所有连接
    ble_send_at_cmd("AT+BLECONN0,0\r\n", 500);
    
    // 停止广播
    if (!ble_send_at_cmd("AT+BLEADVSTOP\r\n", 1000))
    {
        printf("[BLE] Stop advertising failed\n");
        return false;
    }
    
    g_ble_state = BLE_PROV_IDLE;
    printf("[BLE] Advertising stopped\n");
    return true;
}

/**
 * @brief 获取当前 BLE 状态
 */
ble_prov_state_t ble_prov_get_state(void)
{
    return g_ble_state;
}

/**
 * @brief 解析 WiFi 配置数据
 * @param data 格式: "SSID:password" 或 JSON 格式
 */
static bool ble_parse_wifi_config(const char *data, wifi_config_t *config)
{
    if (data == NULL || config == NULL)
        return false;
    
    // 简单格式: "SSID:PASSWORD"
    const char *sep = strchr(data, ':');
    if (sep == NULL)
    {
        printf("[BLE] Invalid config format\n");
        return false;
    }
    
    // 提取 SSID
    int ssid_len = sep - data;
    if (ssid_len <= 0 || ssid_len >= sizeof(config->ssid))
    {
        printf("[BLE] Invalid SSID length\n");
        return false;
    }
    strncpy(config->ssid, data, ssid_len);
    config->ssid[ssid_len] = '\0';
    
    // 提取密码
    strncpy(config->password, sep + 1, sizeof(config->password) - 1);
    config->password[sizeof(config->password) - 1] = '\0';
    
    config->valid = true;
    return true;
}

/**
 * @brief 等待配网数据 (阻塞)
 * @param timeout_ms 超时时间(毫秒)，0 表示无限等待
 * @return true 成功获取配置，false 超时或失败
 */
bool ble_prov_wait_for_config(wifi_config_t *config, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t check_interval = 100;  // 每 100ms 检查一次
    
    printf("[BLE] Waiting for config (timeout: %d ms)...\n", timeout_ms);
    
    while (timeout_ms == 0 || elapsed < timeout_ms)
    {
        // 检查是否有 BLE 写入事件
        // 格式: +WRITE: <conn_index>,<svc_index>,<char_index>,[<len>],<value>
        extern const char *espat_get_response(void);
        const char *resp = espat_get_response();
        
        if (resp != NULL && strstr(resp, "+WRITE:") != NULL)
        {
            printf("[BLE] Received write event: %s\n", resp);
            
            // 解析写入的数据
            const char *data_start = strchr(resp, ',');
            if (data_start != NULL)
            {
                // 跳过前几个逗号，找到数据部分
                for (int i = 0; i < 3 && data_start != NULL; i++)
                {
                    data_start = strchr(data_start + 1, ',');
                }
                
                if (data_start != NULL)
                {
                    data_start++; // 跳过逗号
                    
                    // 尝试解析 WiFi 配置
                    if (ble_parse_wifi_config(data_start, &g_wifi_config))
                    {
                        printf("[BLE] Got WiFi config: SSID=%s\n", g_wifi_config.ssid);
                        
                        if (config != NULL)
                        {
                            memcpy(config, &g_wifi_config, sizeof(wifi_config_t));
                        }
                        
                        g_ble_state = BLE_PROV_SUCCESS;
                        return true;
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval));
        elapsed += check_interval;
    }
    
    printf("[BLE] Wait for config timeout\n");
    g_ble_state = BLE_PROV_FAILED;
    return false;
}

// ==================== WiFi 配置存储 (W25Q16) ====================

/**
 * @brief 计算校验和
 */
static uint8_t calc_checksum(const wifi_config_t *config)
{
    uint8_t sum = 0;
    const uint8_t *p = (const uint8_t *)config;
    
    // 跳过 checksum 字段本身
    for (uint32_t i = 0; i < offsetof(wifi_config_t, checksum); i++)
    {
        sum += p[i];
    }
    
    return sum;
}

/**
 * @brief 保存 WiFi 配置到 W25Q16
 */
bool wifi_config_save(wifi_config_t *config)
{
    if (config == NULL || w25q16 == NULL)
        return false;
    
    // 计算校验和
    config->checksum = calc_checksum(config);
    config->valid = true;
    
    // 先擦除扇区
    w25q16_erase_sector(w25q16, WIFI_CONFIG_ADDR);
    
    // 写入魔数 (标识有效数据)
    uint8_t magic = WIFI_CONFIG_MAGIC;
    w25q16_write_data(w25q16, WIFI_CONFIG_ADDR, &magic, 1);
    
    // 写入配置数据
    w25q16_write_data(w25q16, WIFI_CONFIG_ADDR + 1, 
                      (const uint8_t *)config, sizeof(wifi_config_t));
    
    printf("[W25Q16] WiFi config saved: SSID=%s\n", config->ssid);
    return true;
}

/**
 * @brief 从 W25Q16 加载 WiFi 配置
 */
bool wifi_config_load(wifi_config_t *config)
{
    if (config == NULL || w25q16 == NULL)
        return false;
    
    // 读取魔数
    uint8_t magic = 0;
    w25q16_read_data(w25q16, WIFI_CONFIG_ADDR, &magic, 1);
    
    if (magic != WIFI_CONFIG_MAGIC)
    {
        printf("[W25Q16] No valid config found (magic=0x%02X)\n", magic);
        return false;
    }
    
    // 读取配置数据
    w25q16_read_data(w25q16, WIFI_CONFIG_ADDR + 1, 
                     (uint8_t *)config, sizeof(wifi_config_t));
    
    // 验证校验和
    uint8_t expected = calc_checksum(config);
    if (config->checksum != expected)
    {
        printf("[W25Q16] Checksum mismatch (expected=0x%02X, got=0x%02X)\n", 
               expected, config->checksum);
        memset(config, 0, sizeof(wifi_config_t));
        return false;
    }
    
    printf("[W25Q16] WiFi config loaded: SSID=%s\n", config->ssid);
    return true;
}

/**
 * @brief 清除 WiFi 配置
 */
bool wifi_config_clear(void)
{
    if (w25q16 == NULL)
        return false;
    
    // 擦除配置扇区
    w25q16_erase_sector(w25q16, WIFI_CONFIG_ADDR);
    
    printf("[W25Q16] WiFi config cleared\n");
    return true;
}

/**
 * @brief 检查是否存在有效配置
 */
bool wifi_config_exists(void)
{
    if (w25q16 == NULL)
        return false;
    
    uint8_t magic = 0;
    w25q16_read_data(w25q16, WIFI_CONFIG_ADDR, &magic, 1);
    
    return (magic == WIFI_CONFIG_MAGIC);
}
