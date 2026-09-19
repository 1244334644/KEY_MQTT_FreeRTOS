#ifndef __BLE_PROV_H__
#define __BLE_PROV_H__

#include <stdint.h>
#include <stdbool.h>

// WiFi 配置结构体
typedef struct {
    char ssid[64];
    char password[64];
    bool valid;          // 配置是否有效
    uint8_t checksum;    // 校验和
} wifi_config_t;

// BLE 配网状态
typedef enum {
    BLE_PROV_IDLE,           // 空闲
    BLE_PROV_WAITING,        // 等待配网
    BLE_PROV_CONNECTED,      // BLE已连接
    BLE_PROV_RECEIVING,      // 正在接收数据
    BLE_PROV_SUCCESS,        // 配网成功
    BLE_PROV_FAILED          // 配网失败
} ble_prov_state_t;

// BLE 配网结果回调
typedef void (*ble_prov_callback_t)(wifi_config_t *config, bool success);

// 核心函数
bool ble_prov_init(void);
bool ble_prov_start(void);
bool ble_prov_stop(void);
ble_prov_state_t ble_prov_get_state(void);
bool ble_prov_wait_for_config(wifi_config_t *config, uint32_t timeout_ms);

// WiFi 配置存储 (W25Q16)
// 使用用户数据区，避免与字体/图片/索引表冲突
#define WIFI_CONFIG_ADDR  0x1E0000  // 存储起始地址 (用户数据区)
#define WIFI_CONFIG_SIZE  0x100     // 256字节足够
#define WIFI_CONFIG_MAGIC 0xAA      // 魔数标识

bool wifi_config_save(wifi_config_t *config);
bool wifi_config_load(wifi_config_t *config);
bool wifi_config_clear(void);
bool wifi_config_exists(void);

#endif // __BLE_PROV_H__
