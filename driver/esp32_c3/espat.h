#ifndef __ESPAT_H__
#define __ESPAT_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	char ssid[64];
	char bssid[18];
	int channel;
	int rssi;
	bool connected;
} esp_wifi_info_t;

typedef struct 
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t weekday;
} esp_time_t;

/* --- 原有基础函数 --- */
bool espat_init();
bool espat_wifi_init();
bool espat_connect_wifi(const char *ssid, const char *password, const char *mac);
bool espat_get_wifi_info(esp_wifi_info_t *info);
bool wifi_is_connected();
void esp32_SendData(unsigned char *data, unsigned short len);
void ESP32_Clear(void);
unsigned char *ESP32_GetIPD(unsigned short timeOut);
bool espat_write_command(const char *command, uint32_t timeout);
void espat_usart_write_data(const char *data, uint16_t len);
#endif /* __ESPat_H__ */