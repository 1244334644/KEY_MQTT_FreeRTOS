/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	onenet.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2017-05-08
	*
	*	版本： 		V1.1
	*
	*	说明： 		与onenet平台的数据交互接口层
	*
	*	修改记录：	V1.0：协议封装、返回判断都在同一个文件，并且不同协议接口不同。
	*				V1.1：提供统一接口供应用层使用，根据不同协议文件来封装协议相关的内容。
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f4xx.h"

//网络设备
#include "espat.h"

//协议文件
#include "onenet.h"
#include "mqttkit.h"
#include "cJSON.h"

//算法
#include "base64.h"
#include "hmac_sha1.h"

//硬件驱动
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "led.h"

//C库
#include <string.h>
#include <stdio.h>


#define PROID			"84q8r448p5"

#define ACCESS_KEY		"QWZ6SmdHSWZ6d0IxYThBR1BvQU0xZ1ladVFVWEhLS24="

#define DEVICE_NAME		"key"

// 请确保 PROID 和 DEVICE_NAME 已定义
#define POST_TOPIC  "$sys/" PROID "/" DEVICE_NAME "/thing/property/post"
char devid[16];

char key[48];



/*
************************************************************
*	函数名称：	OTA_UrlEncode
*
*	函数功能：	sign需要进行URL编码
*
*	入口参数：	sign：加密结果
*
*	返回参数：	0-成功	其他-失败
*
*	说明：		+			%2B
*				空格		%20
*				/			%2F
*				?			%3F
*				%			%25
*				#			%23
*				&			%26
*				=			%3D
************************************************************
*/
void OneNET_Publish(const char *topic, const char *msg);
static unsigned char OTA_UrlEncode(char *sign)
{

	char sign_t[40];
	unsigned char i = 0, j = 0;
	unsigned char sign_len = strlen(sign);
	
	if(sign == (void *)0 || sign_len < 28)
		return 1;
	
	for(; i < sign_len; i++)
	{
		sign_t[i] = sign[i];
		sign[i] = 0;
	}
	sign_t[i] = 0;
	
	for(i = 0, j = 0; i < sign_len; i++)
	{
		switch(sign_t[i])
		{
			case '+':
				strcat(sign + j, "%2B");j += 3;
			break;
			
			case ' ':
				strcat(sign + j, "%20");j += 3;
			break;
			
			case '/':
				strcat(sign + j, "%2F");j += 3;
			break;
			
			case '?':
				strcat(sign + j, "%3F");j += 3;
			break;
			
			case '%':
				strcat(sign + j, "%25");j += 3;
			break;
			
			case '#':
				strcat(sign + j, "%23");j += 3;
			break;
			
			case '&':
				strcat(sign + j, "%26");j += 3;
			break;
			
			case '=':
				strcat(sign + j, "%3D");j += 3;
			break;
			
			default:
				sign[j] = sign_t[i];j++;
			break;
		}
	}
	
	sign[j] = 0;
	
	return 0;

}

/*
************************************************************
*	函数名称：	OTA_Authorization
*
*	函数功能：	计算Authorization
*
*	入口参数：	ver：参数组版本号，日期格式，目前仅支持格式"2018-10-31"
*				res：产品id
*				et：过期时间，UTC秒值
*				access_key：访问密钥
*				dev_name：设备名
*				authorization_buf：缓存token的指针
*				authorization_buf_len：缓存区长度(字节)
*
*	返回参数：	0-成功	其他-失败
*
*	说明：		当前仅支持sha1
************************************************************
*/
#define METHOD		"sha1"
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
											char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
{
	
	size_t olen = 0;
	
	char sign_buf[64];								//保存签名的Base64编码结果 和 URL编码结果
	char hmac_sha1_buf[64];							//保存签名
	char access_key_base64[64];						//保存access_key的Base64编码结合
	char string_for_signature[72];					//保存string_for_signature，这个是加密的key

//----------------------------------------------------参数合法性--------------------------------------------------------------------
	if(ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0
		|| authorization_buf == (void *)0 || authorization_buf_len < 120)
		return 1;
	
//----------------------------------------------------将access_key进行Base64解码----------------------------------------------------
	memset(access_key_base64, 0, sizeof(access_key_base64));
	BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &olen, (unsigned char *)access_key, strlen(access_key));
//	UsartPrintf(USART_DEBUG, "access_key_base64: %s\r\n", access_key_base64);
	
//----------------------------------------------------计算string_for_signature-----------------------------------------------------
	memset(string_for_signature, 0, sizeof(string_for_signature));
	if(flag)
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
	else
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);
//	UsartPrintf(USART_DEBUG, "string_for_signature: %s\r\n", string_for_signature);
	
//----------------------------------------------------加密-------------------------------------------------------------------------
	memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));
	hmac_sha1((unsigned char *)access_key_base64, strlen(access_key_base64),
				(unsigned char *)string_for_signature, strlen(string_for_signature),
				(unsigned char *)hmac_sha1_buf);
	
//	UsartPrintf(USART_DEBUG, "hmac_sha1_buf: %s\r\n", hmac_sha1_buf);
	
//----------------------------------------------------将加密结果进行Base64编码------------------------------------------------------
	olen = 0;
	memset(sign_buf, 0, sizeof(sign_buf));
	BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, strlen(hmac_sha1_buf));

//----------------------------------------------------将Base64编码结果进行URL编码---------------------------------------------------
	OTA_UrlEncode(sign_buf);
//	UsartPrintf(USART_DEBUG, "sign_buf: %s\r\n", sign_buf);
	
//----------------------------------------------------计算Token--------------------------------------------------------------------
	if(flag)
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
	else
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);
//	UsartPrintf(USART_DEBUG, "Token: %s\r\n", authorization_buf);
	
	return 0;

}

//==========================================================
//	函数名称：	OneNET_RegisterDevice
//
//	函数功能：	在产品中注册一个设备
//
//	入口参数：	access_key：访问密钥
//				pro_id：产品ID
//				serial：唯一设备号
//				devid：保存返回的devid
//				key：保存返回的key
//
//	返回参数：	0-成功		1-失败
//
//	说明：		
//==========================================================
_Bool OneNET_RegisterDevice(void)
{

	_Bool result = 1;
	unsigned short send_len = 11 + strlen(DEVICE_NAME);
	char *send_ptr = NULL, *data_ptr = NULL;
	
	char authorization_buf[144];													//加密的key
	
	send_ptr = malloc(send_len + 240);
	if(send_ptr == NULL)
		return result;
	
	while(espat_write_command("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80", 2000));
	
	OneNET_Authorization("2025-12-13", PROID, 1785936599, ACCESS_KEY, NULL,
							authorization_buf, sizeof(authorization_buf), 1);
	
	snprintf(send_ptr, 240 + send_len, "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
					"Authorization:%s\r\n"
					"Host:ota.heclouds.com\r\n"
					"Content-Type:application/json\r\n"
					"Content-Length:%d\r\n\r\n"
					"{\"name\":\"%s\"}",
	
					authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);
	
	esp32_SendData((unsigned char *)send_ptr, strlen(send_ptr));
	
	/*
	{
	  "request_id" : "f55a5a37-36e4-43a6-905c-cc8f958437b0",
	  "code" : "onenet_common_success",
	  "code_no" : "000000",
	  "message" : null,
	  "data" : {
		"device_id" : "589804481",
		"name" : "mcu_id_43057127",
		
	"pid" : 282932,
		"key" : "indu/peTFlsgQGL060Gp7GhJOn9DnuRecadrybv9/XY="
	  }
	}
	*/
	
	data_ptr = (char *)ESP32_GetIPD(250);							//等待平台响应
	
	if(data_ptr)
	{
		data_ptr = strstr(data_ptr, "device_id");
	}
	
	if(data_ptr)
	{
		char name[16];
		int pid = 0;
		
		if(sscanf(data_ptr, "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"", devid, name, &pid, key) == 4)
		{
			UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
			result = 0;
		}
	}
	
	free(send_ptr);
	espat_write_command("AT+CIPCLOSE\r\n", 1000);
	
	return result;

}

//==========================================================
//	函数名称：	OneNet_DevLink
//
//	函数功能：	与onenet创建连接
//
//	入口参数：	无
//
//	返回参数：	1-成功	0-失败
//
//	说明：		与onenet平台建立连接
//==========================================================
// ... 前面的代码保持不变 ...

//==========================================================
//	函数名称：	OneNet_DevLink
//
//	函数功能：	与onenet创建连接
//
//	入口参数：	无
//
//	返回参数：	1-失败	0-成功
//
//	说明：		与onenet平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};

	unsigned char *dataPtr;
	char authorization_buf[256]; // 稍微改大一点防止溢出
	_Bool status = 1;

    // 1. 【新增步骤】先建立 TCP 连接！
    // 只有连上了服务器，才能发送 CIPSEND
    // OneNet 新版 MQTT 地址: mqtts.heclouds.com, 端口 1883
    UsartPrintf(USART_DEBUG, "Tips: Start TCP Connect...\r\n");
    if(!espat_write_command("AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n", 5000))
    {
        UsartPrintf(USART_DEBUG, "WARN: TCP Connect Failed\r\n");
        return 1; // 返回失败
    }
    UsartPrintf(USART_DEBUG, "Tips: TCP Connected\r\n");
	vTaskDelay(pdMS_TO_TICKS(1000));
    // 2. 计算鉴权 Token (保持原样)
	OneNET_Authorization("2018-10-31", PROID, 1785936599, ACCESS_KEY, DEVICE_NAME,
								authorization_buf, sizeof(authorization_buf), 0);
	
	UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
							"NAME: %s,	PROID: %s,	KEY:%s\r\n"
                        , DEVICE_NAME, PROID, authorization_buf);
	
    // 3. 封包并发送 CONNECT 报文
	if(MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		esp32_SendData(mqttPacket._data, mqttPacket._len);			//上传平台
		
		dataPtr = ESP32_GetIPD(500); // 稍微增加等待时间
		if(dataPtr != NULL)
		{
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n");status = 0;break;
					case 1:UsartPrintf(USART_DEBUG, "WARN:	连接失败：协议错误\r\n");break;
					case 2:UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法的clientid\r\n");break;
					case 3:UsartPrintf(USART_DEBUG, "WARN:	连接失败：服务器失败\r\n");break;
					case 4:UsartPrintf(USART_DEBUG, "WARN:	连接失败：用户名或密码错误\r\n");break;
					case 5:UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法链接(比如token非法)\r\n");break;
					default:UsartPrintf(USART_DEBUG, "ERR:	连接失败：未知错误\r\n");break;
				}
			}
		}
		
		MQTT_DeleteBuffer(&mqttPacket);								//删包
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");
	
	return status;
}
//{"id":"1","version":"1.0","params":{"key1_state":true,"key2_state":false,"led1_state":false,"led2_state":true}}
//"{\"id\":\"123\",\"params\":{")
extern bool key1_state, key2_state, led1_state, led2_state;

// 【新增】状态改变标志，用于通知主循环立即上报
bool state_changed = false;
char g_city[32] = "guangzhou";
char g_location[128] = "panyu";
// 【新增】全局变量存储温湿度数据，由inner_update()更新
float g_temperature = 0.0f;

float g_inner_temp = 0.0f;
float g_inner_humi = 0.0f;
float g_temp_over = 25.0f;
float g_humi_over = 60.0f;

int weather_code = 0; // 天气代码，0=晴天，1=多云，2=雨天，3=雪天

unsigned char OneNet_FillBuf(char *buf)
{
	
	char text[128];
	strcpy(buf, "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{");
	
	// ========== weather物模型数据（嵌套对象）==========
	memset(text, 0, sizeof(text));
	sprintf(text, "\"weather\":{\"value\":{\"city\":\"%s\",\"location\":\"%s\",\"weather\":%d,\"temp\":%.1f}},", 
		g_city, g_location, weather_code, g_temperature);
	strcat(buf, text);
	
	// ========== 内部温湿度（AHT20传感器数据）==========
	memset(text, 0, sizeof(text));
	sprintf(text, "\"inner_temp\":{\"value\":%.1f},", g_inner_temp);
	strcat(buf, text);
	
	memset(text, 0, sizeof(text));
	sprintf(text, "\"inner_humi\":{\"value\":%.1f},", g_inner_humi);
	strcat(buf, text);

	// ========== 设置温湿度上限（AHT20传感器数据）==========
	memset(text, 0, sizeof(text));
	sprintf(text, "\"temp_over\":{\"value\":%.1f},", g_temp_over);
	strcat(buf, text);
	
	memset(text, 0, sizeof(text));
	sprintf(text, "\"humi_over\":{\"value\":%.1f},", g_humi_over);
	strcat(buf, text);
	
	// ========== LED状态 ==========
	memset(text, 0, sizeof(text));
	sprintf(text, "\"led1_state\":{\"value\":%s},", led1_state ? "true" : "false");
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"led2_state\":{\"value\":%s},", led2_state ? "true" : "false");
	strcat(buf, text);

	// ========== 按键状态 ==========
	memset(text, 0, sizeof(text));
	sprintf(text, "\"key1_state\":{\"value\":%s},", key1_state ? "true" : "false");
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"key2_state\":{\"value\":%s}", key2_state ? "true" : "false");  // 最后一个参数不加逗号
	strcat(buf, text);
	
	strcat(buf, "}}");
	
	// 【安全检查】打印JSON长度，防止缓冲区溢出
	unsigned int json_len = strlen(buf);
	UsartPrintf(USART_DEBUG, "[JSON] Length: %d bytes\r\n", json_len);
	if(json_len > 500)
	{
		UsartPrintf(USART_DEBUG, "[JSON] WARNING: JSON too long! (%d > 500)\r\n", json_len);
	}
	
	return json_len;

}

//==========================================================
//	函数名称：	OneNet_SendData
//
//	函数功能：	上传数据到平台
//
//	入口参数：	type：发送数据的格式
//
//	返回参数：	无
//
//	说明：		
//==========================================================
//==========================================================
//	函数名称：	OneNet_SendData
//	函数功能：	上传数据到平台 (物模型属性上报)
//==========================================================
void OneNet_SendData(void)
{
	char buf[512];  // 【修复】增大缓冲区以容纳完整JSON
	
	memset(buf, 0, sizeof(buf));
	
	// 【调试】打印当前状态
	UsartPrintf(USART_DEBUG, "[SEND] Current State: LED1=%d, LED2=%d, KEY1=%d, KEY2=%d\r\n", 
		led1_state, led2_state, key1_state, key2_state);
	
	// 1. 填充 JSON 数据
	if(OneNet_FillBuf(buf) > 0)
	{
        // 【优化】不打印完整JSON，避免缓冲区溢出，只打印长度和前80字符
        unsigned int json_len = strlen(buf);
        UsartPrintf(USART_DEBUG, "[SEND] JSON Length: %d bytes\r\n", json_len);
        
        // 【调试】分段打印JSON（前100字符、中间、最后100字符）
        if(json_len > 200)
        {
            char temp[101];
            memcpy(temp, buf, 100);
            temp[100] = '\0';
            UsartPrintf(USART_DEBUG, "[SEND] JSON Start: %s...\r\n", temp);
            
            memcpy(temp, buf + json_len - 100, 100);
            temp[100] = '\0';
            UsartPrintf(USART_DEBUG, "[SEND] JSON End: ...%s\r\n", temp);
        }
        else
        {
            UsartPrintf(USART_DEBUG, "[SEND] JSON: %s\r\n", buf);
        }

        // 2. 直接调用 Publish 发送到 物模型 Topic
        // 这会自动封装 MQTT 头、计算长度并发送
        OneNET_Publish(POST_TOPIC, buf);
        
        // 【调试】确认发送完成
        UsartPrintf(USART_DEBUG, "[SEND] Data sent successfully\r\n");
	}
	else
	{
		UsartPrintf(USART_DEBUG, "[SEND] ERROR: Failed to fill buffer\r\n");
	}
}

//==========================================================
//	函数名称：	OneNET_Publish
//
//	函数功能：	发布消息
//
//	入口参数：	topic：发布的主题
//				msg：消息内容
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void OneNET_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						//协议包
	
	// 【优化】不打印完整消息，只打印长度和topic
	unsigned int msg_len = strlen(msg);
	UsartPrintf(USART_DEBUG, "[PUB] Topic: %s, Msg Length: %d\r\n", topic, msg_len);
	
	uint8_t result = MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, msg_len, MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet);
	if(result == 0)
	{
		if(mqtt_packet._data != NULL)
		{
			UsartPrintf(USART_DEBUG, "[PUB] MQTT Packet: %d bytes\r\n", mqtt_packet._len);
			esp32_SendData(mqtt_packet._data, mqtt_packet._len);					//向平台发送订阅请求
			MQTT_DeleteBuffer(&mqtt_packet);										//删包
		}
		else
		{
			UsartPrintf(USART_DEBUG, "[PUB] ERROR: MQTT packet data is NULL\r\n");
		}
	}
	else
	{
		UsartPrintf(USART_DEBUG, "[PUB] ERROR: MQTT_PacketPublish failed, code=%d\r\n", result);
	}

}

//==========================================================
//	函数名称：	OneNET_Subscribe
//
//	函数功能：	订阅
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void OneNET_Subscribe(void)
{
	
	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						//协议包
	
	char topic_buf[56];
	const char *topic = topic_buf;
	
	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);
	
	UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);
	
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, &topic, 1, &mqtt_packet) == 0)
	{
		esp32_SendData(mqtt_packet._data, mqtt_packet._len);					//向平台发送订阅请求
		
		MQTT_DeleteBuffer(&mqtt_packet);										//删包
	}

}

//==========================================================
//	函数名称：	OneNet_RevPro
//
//	函数功能：	平台返回数据检测
//
//	入口参数：	dataPtr：平台返回的数据
//
//	返回参数：	无
//
//	说明：		
//==========================================================
extern led_desc_t led1, led2;
void OneNet_RevPro(unsigned char *cmd)
{
	
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	
	unsigned short topic_len = 0;
	unsigned short req_len = 0;
	
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;
	
	unsigned char type = 0;
	
	short result = 0;

	// char *dataPtr = NULL;
	// char numBuf[10];
	// int num = 0;
	
	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_PUBLISH:																//接收的Publish消息
		
			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0)
			{
				//char *data_ptr = NULL;
				
				UsartPrintf(USART_DEBUG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																	cmdid_topic, topic_len, req_payload, req_len);
				
				cJSON *raw_json = cJSON_Parse(req_payload);
				if(raw_json == NULL)
				{
					UsartPrintf(USART_DEBUG, "WARN: JSON Parse Failed\r\n");
					return;
				}
				
				cJSON *params_json = cJSON_GetObjectItem(raw_json,"params");
				if(params_json == NULL)
				{
					UsartPrintf(USART_DEBUG, "WARN: No 'params' field in JSON\r\n");
					cJSON_Delete(raw_json);
					return;
				}
				
				// OneNet物模型下发的数据格式可能是:
				// 1. {"params": {"led1_state": true}}  直接布尔值
				// 2. {"params": {"led1_state": {"value": true}}}  带value字段
				
				cJSON *led1_json = cJSON_GetObjectItem(params_json,"led1_state");
				cJSON *led2_json = cJSON_GetObjectItem(params_json,"led2_state");
				cJSON *temp_over_json = cJSON_GetObjectItem(params_json, "temp_over");
				cJSON *humi_over_json = cJSON_GetObjectItem(params_json, "humi_over");

				// =================处理温度上限设置=================
				if(temp_over_json != NULL)
				{
					double val = 0;
					bool valid = false;

					// 1. 直接数值格式: {"temp_over": 30.5}
					if(temp_over_json->type == cJSON_Number)
					{
						val = temp_over_json->valuedouble;
						valid = true;
					}
					// 2. 嵌套对象格式: {"temp_over": {"value": 30.5}}
					else if(temp_over_json->type == cJSON_Object)
					{
						cJSON *value_obj = cJSON_GetObjectItem(temp_over_json, "value");
						if(value_obj != NULL && value_obj->type == cJSON_Number)
						{
							val = value_obj->valuedouble;
							valid = true;
						}
					}

					if(valid)
					{
						g_temp_over = (float)val;
						UsartPrintf(USART_DEBUG, "[CMD] Set Temp Limit: %.1f\r\n", g_temp_over);
						// 立即触发上报以同步状态
						// extern bool state_changed; 
						// state_changed = true; 
					}
				}

				// =================处理湿度上限设置=================
				if(humi_over_json != NULL)
				{
					double val = 0;
					bool valid = false;

					// 1. 直接数值格式
					if(humi_over_json->type == cJSON_Number)
					{
						val = humi_over_json->valuedouble;
						valid = true;
					}
					// 2. 嵌套对象格式
					else if(humi_over_json->type == cJSON_Object)
					{
						cJSON *value_obj = cJSON_GetObjectItem(humi_over_json, "value");
						if(value_obj != NULL && value_obj->type == cJSON_Number)
						{
							val = value_obj->valuedouble;
							valid = true;
						}
					}

					if(valid)
					{
						g_humi_over = (float)val;
						UsartPrintf(USART_DEBUG, "[CMD] Set Humi Limit: %.1f\r\n", g_humi_over);
					}
				}
				// 处理LED1
				if(led1_json != NULL)
				{
					_Bool led1_value = 0;
					
					// 检查是直接值还是嵌套对象
					if(led1_json->type == cJSON_True || led1_json->type == cJSON_False)
					{
						// 直接布尔值: {"led1_state": true}
						led1_value = (led1_json->type == cJSON_True);
						UsartPrintf(USART_DEBUG, "LED1 direct value: %d\r\n", led1_value);
					}
					else if(led1_json->type == cJSON_Object)
					{
						// 嵌套对象: {"led1_state": {"value": true}}
						cJSON *value_obj = cJSON_GetObjectItem(led1_json, "value");
						if(value_obj != NULL)
						{
							led1_value = (value_obj->type == cJSON_True);
							UsartPrintf(USART_DEBUG, "LED1 nested value: %d\r\n", led1_value);
						}
					}
					else if(led1_json->type == cJSON_Number)
					{
						// 数值: {"led1_state": 1}
						led1_value = (led1_json->valueint != 0);
						UsartPrintf(USART_DEBUG, "LED1 number value: %d\r\n", led1_value);
					}
					
					// 【优化】先检查状态是否真的改变了，避免无效操作
					if(led1_state != led1_value)
					{
						// 控制LED1
						if(led1_value)
						{
							led_on(led1);
							led1_state = 1;
							UsartPrintf(USART_DEBUG, "LED1 turned ON\r\n");
						}
						else
						{
							led_off(led1);
							led1_state = 0;
							UsartPrintf(USART_DEBUG, "LED1 turned OFF\r\n");
						}
						
						// 【优化】设置标志，让主循环快速上报，提高响应速度
						state_changed = true;
					}
					else
					{
						UsartPrintf(USART_DEBUG, "LED1 state unchanged, skip operation\r\n");
					}
				}
				
				// 处理LED2
				if(led2_json != NULL)
				{
					_Bool led2_value = 0;
					
					// 检查是直接值还是嵌套对象
					if(led2_json->type == cJSON_True || led2_json->type == cJSON_False)
					{
						// 直接布尔值
						led2_value = (led2_json->type == cJSON_True);
						UsartPrintf(USART_DEBUG, "LED2 direct value: %d\r\n", led2_value);
					}
					else if(led2_json->type == cJSON_Object)
					{
						// 嵌套对象
						cJSON *value_obj = cJSON_GetObjectItem(led2_json, "value");
						if(value_obj != NULL)
						{
							led2_value = (value_obj->type == cJSON_True);
							UsartPrintf(USART_DEBUG, "LED2 nested value: %d\r\n", led2_value);
						}
					}
					else if(led2_json->type == cJSON_Number)
					{
						// 数值
						led2_value = (led2_json->valueint != 0);
						UsartPrintf(USART_DEBUG, "LED2 number value: %d\r\n", led2_value);
					}
					
					// 【优化】先检查状态是否真的改变了，避免无效操作
					if(led2_state != led2_value)
					{
						// 控制LED2
						if(led2_value)
						{
							led_on(led2);
							led2_state = 1;
							UsartPrintf(USART_DEBUG, "LED2 turned ON\r\n");
						}
						else
						{
							led_off(led2);
							led2_state = 0;
							UsartPrintf(USART_DEBUG, "LED2 turned OFF\r\n");
						}
						
						// 【优化】设置标志，让主循环快速上报，提高响应速度
						state_changed = true;
					}
					else
					{
						UsartPrintf(USART_DEBUG, "LED2 state unchanged, skip operation\r\n");
					}
				}
				
				cJSON_Delete(raw_json);
				
				
//				data_ptr = strstr(cmdid_topic, "request/");									//查找cmdid
//				if(data_ptr)
//				{
//					char topic_buf[80], cmdid[40];
//					
//					data_ptr = strchr(data_ptr, '/');
//					data_ptr++;
//					
//					memcpy(cmdid, data_ptr, 36);											//复制cmdid
//					cmdid[36] = 0;
//					
//					snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/cmd/response/%s",
//															PROID, DEVICE_NAME, cmdid);
//					OneNET_Publish(topic_buf, "ojbk");										//回复命令
//				}
			}
			
		case MQTT_PKT_PUBACK:														//发送Publish消息，平台回复的Ack
		
			if(MQTT_UnPacketPublishAck(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Publish Send OK\r\n");
			
		break;
		
		case MQTT_PKT_SUBACK:																//发送Subscribe消息的Ack
		
			if(MQTT_UnPacketSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe Err\r\n");
		
		break;
		
		default:
			result = -1;
		break;
	}
	
	ESP32_Clear();									//清空缓存
	
	if(result == -1)
		return;
	
//	dataPtr = strchr(req_payload, ':');					//搜索':'

//	if(dataPtr != NULL && result != -1)					//如果找到了
//	{
//		dataPtr++;
//		
//		while(*dataPtr >= '0' && *dataPtr <= '9')		//判断是否是下发的命令控制数据
//		{
//			numBuf[num++] = *dataPtr++;
//		}
//		numBuf[num] = 0;
//		
//		num = atoi((const char *)numBuf);				//转为数值形式
//	}
	

	
	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}

}
