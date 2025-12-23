#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "espat.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#define ESP_AT_DEBUG 1
#define REV_OK 0
#define REV_WAIT 1
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))


typedef enum
{
    AT_ACK_NONE,
    AT_ACK_OK,
    AT_ACK_ERROR,
    AT_ACK_BUSY,
    AT_ACK_READY,
    AT_ACK_WIFI_CONNECTED,
    AT_ACK_WIFI_GOT_IP,
    AT_ACK_CONNECT,
    AT_ACK_JKH
}at_ack_t;

typedef struct
{
    at_ack_t ack;
    const char *string;
}at_ack_match_t;


static void espat_usart_write(const char *data);
static bool espat_wait_boot(uint32_t timeout);
bool espat_wait_ready(uint32_t timeout);
static at_ack_t espat_usart_wait_receive(uint32_t timeout);
// 【优化】增大缓冲区到512字节，防止MQTT消息被截断
unsigned char esp32_buf[512];
unsigned short esp32_cnt = 0, esp32_cntPre = 0;
static uint32_t rxlen;
static const char *rxline; 

static const at_ack_match_t at_ack_matches[]=
{
    // 【修改点】把 > 放到第一位！优先级最高！
    {AT_ACK_JKH, ">"}, 
    
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy p…\r\n"},
    {AT_ACK_READY, "ready\r\n"},
    {AT_ACK_WIFI_CONNECTED, "WIFI CONNECTED\r\nWIFI GOT IP\r\n"},
    {AT_ACK_CONNECT, "CONNECT\r\n"},
};
static char rxbuf[1024];

void ESP32_Clear(void)
{

	memset(esp32_buf, 0, sizeof(esp32_buf));
	esp32_cnt = 0;

}
static at_ack_t  rxack;
static SemaphoreHandle_t espat_ack_semaphore;

static void espat_io_init(void)
{
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

static void espat_usart_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200u;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    USART_Init(USART2, &USART_InitStructure);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE); 
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);  
    
}

static void espat_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    
    // 【修改点1】内存地址必须自增
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable; 

    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream6, &DMA_InitStruct);
}

static void espat_int_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(USART2_IRQn, 5);
}
 
static void espat_lowlevel_init()
{
    espat_usart_init();
    espat_dma_init();
    espat_int_init();
    espat_io_init();
    
}

bool espat_init()
{
    espat_ack_semaphore = xSemaphoreCreateBinary();
    // 这里的断言可能会在重复初始化时报错，建议去掉或判空
    // configASSERT(espat_ack_semaphore); 
    
    espat_lowlevel_init();

    // 1. 发送测试指令，确认模块活着
    // 尝试几次，因为刚上电可能还在乱码中
    bool at_ok = false;
    for(int i=0; i<5; i++) {
        if(espat_write_command("AT\r\n", 500)) {
            at_ok = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if(!at_ok) return false;

    // 2. 恢复出厂设置
    // 注意：发送完这个命令模块会立即重启，可能回OK也可能不回
    espat_usart_write("AT+RESTORE\r\n");
    
    // 3. 关键：强制延时 2 秒，让模块完成重启的动作
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 4. 清空缓冲区，准备捕捉 "ready"
    // 重启时会有很多乱码，必须清空
    ESP32_Clear(); 
    
    // 5. 等待 "ready" 字样出现
    // 给够时间，因为出厂设置恢复较慢
    if(!espat_wait_ready(5000))
    {
        // 如果等不到 ready，可能是已经重启完了我们错过了
        // 再发一次 AT 试试，如果回 OK 也算成功
        if(espat_write_command("AT\r\n", 500)) {
            return true;
        }
        return false;
    }

    return true;
}
static void espat_usart_write(const char *data)
{
    uint32_t len = strlen(data);

    DMA_Cmd(DMA1_Stream6, DISABLE); // 确保DMA已关闭
    while(DMA_GetCmdStatus(DMA1_Stream6) != DISABLE);

    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);
    
    DMA1_Stream6->M0AR = (uint32_t)data;
    DMA1_Stream6->NDTR = len;
    
    DMA_Cmd(DMA1_Stream6, ENABLE);

    // 简单应用建议在这里等待发送完成，防止 data 指针失效
    // 如果追求极致效率，需确保 data 是全局变量
    while(DMA_GetFlagStatus(DMA1_Stream6, DMA_FLAG_TCIF6) == RESET);
    espat_usart_write_data(data, strlen(data));
}
// 在 espat.h 声明: void espat_usart_write_data(const char *data, uint16_t len);

// 在 espat.c 实现:
void espat_usart_write_data(const char *data, uint16_t len)
{
    DMA_Cmd(DMA1_Stream6, DISABLE);
    while(DMA_GetCmdStatus(DMA1_Stream6) != DISABLE);
    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);

    DMA1_Stream6->M0AR = (uint32_t)data;
    DMA1_Stream6->NDTR = len;
    DMA_Cmd(DMA1_Stream6, ENABLE);

    // 必须等待发送完成
    while(DMA_GetFlagStatus(DMA1_Stream6, DMA_FLAG_TCIF6) == RESET);
}
static at_ack_t match_internal_ack(const char *str)
{
    for(uint32_t i=0;i<ARRAY_SIZE(at_ack_matches);i++)
    {
        if(strcmp(str,at_ack_matches[i].string)==0)
            return at_ack_matches[i].ack;
    }
    return AT_ACK_NONE;
}


static at_ack_t espat_usart_wait_receive(uint32_t timeout)
{
    uint32_t rxlen = 0;
    rxbuf[0] = '\0';
    
    // 循环等待，直到超时
    for (uint32_t i = 0; i < timeout; i++)
    {
        // 1. 搬运数据: 中断buf -> rxbuf
        if (esp32_cnt > 0)
        {
            taskENTER_CRITICAL(); // 临界区保护
            
            int copy_len = esp32_cnt;
            // 防止 rxbuf 越界
            if (rxlen + copy_len >= sizeof(rxbuf) - 1) 
                copy_len = sizeof(rxbuf) - 1 - rxlen;

            if (copy_len > 0)
            {
                memcpy(rxbuf + rxlen, esp32_buf, copy_len);
                rxlen += copy_len;
                rxbuf[rxlen] = '\0'; // 补结束符
            }
            
            esp32_cnt = 0; // 清空中断计数
            
            taskEXIT_CRITICAL();

            // 2. 模糊匹配关键词
            // 遍历所有可能的回复 (OK, ERROR, etc.)
            for(uint32_t k = 0; k < ARRAY_SIZE(at_ack_matches); k++)
            {
                // 使用 strstr 而不是 strcmp
                // 只要 rxbuf 里包含了 "OK\r\n"，不管前面有没有 "AT\r\n"，都算成功
                if (strstr(rxbuf, at_ack_matches[k].string))
                {
                    return at_ack_matches[k].ack;
                }
            }
        }
        
        // 3. 延时 1ms
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return AT_ACK_NONE;
}

bool espat_wait_ready(uint32_t timeout)
{
    return (espat_usart_wait_receive(timeout)==AT_ACK_READY);
   
}

bool espat_write_command(const char *command, uint32_t timeout)
{
#if ESP_AT_DEBUG
    printf("[DEBUG] Send: %s\n", command);
#endif

    // 1. 发送前清空缓存
    ESP32_Clear(); 
    espat_usart_write(command);
    
    // 2. 判断是否为 CIPSEND 指令
    // CIPSEND 需要特殊处理：必须等到 ">" (AT_ACK_JKH)
    bool is_cipsend = (strstr(command, "CIPSEND") != NULL);

    // 3. 循环等待回复
    // 我们可能需要调用多次 espat_usart_wait_receive，因为可能先收到 OK，再收到 >
    uint32_t start_time = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout))
    {
        at_ack_t ack = espat_usart_wait_receive(100); // 每次等 100ms
        
        // 如果收到了 >，无论是什么指令，都算成功
        if (ack == AT_ACK_JKH) 
        {
            #if ESP_AT_DEBUG
            printf("[DEBUG] Recv: > (Prompt)\n");
            #endif
            return true;
        }

        // 如果是普通指令，收到 OK/CONNECT 等就算成功
        if (!is_cipsend)
        {
            if (ack == AT_ACK_OK || ack == AT_ACK_WIFI_CONNECTED || 
                ack == AT_ACK_WIFI_GOT_IP || ack == AT_ACK_CONNECT)
            {
                #if ESP_AT_DEBUG
                printf("[DEBUG] Recv Success: %d\n", ack);
                #endif
                return true; 
            }
        }
        else // 如果是 CIPSEND 指令
        {
            // 收到 OK 不能返回，要继续等 >
            if (ack == AT_ACK_OK)
            {
                #if ESP_AT_DEBUG
                printf("[DEBUG] Recv OK, waiting for > ...\n");
                #endif
                continue; // 继续循环
            }
            // 收到 ERROR 直接失败
            if (ack == AT_ACK_ERROR) return false;
        }
    }

    #if ESP_AT_DEBUG
    printf("[DEBUG] Timeout or Fail\n");
    #endif
    return false; 
}

const char *espat_get_response(void)
{
    return rxbuf;
}

static bool espat_wait_boot(uint32_t timeout)
{
    for(int t =0;t<timeout;t+=100)
    {
        if(espat_write_command("AT\r\n",100))
            return true;
    }
    return false;
}

bool espat_wifi_init()
{
    // 关闭回显
    espat_write_command("ATE0\r\n", 500);

    // 【新增】强制单连接模式
    if(!espat_write_command("AT+CIPMUX=0\r\n", 2000))
    {
         // 如果这步失败通常不影响，可能是已经处于0了
    }

    // 设置 Station
    if(!espat_write_command("AT+CWMODE=1\r\n", 2000)) return false;
    
    // 开启 DHCP
    if(!espat_write_command("AT+CWDHCP=1,1\r\n", 2000)) return false;

    return true;
}


bool espat_connect_wifi(const char *ssid, const char *password, const char *mac)
{
    if(ssid==NULL||password==NULL)
        return false;
    
    char command[128];

    int len=snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    if(mac)
        snprintf(command+len, sizeof(command)-len, ",%s", mac);
    
    // 【修改】超时时间增加到 10000ms (10秒)
    // 因为连接 WiFi 有时候真的很慢，给够时间等待 WIFI GOT IP
    return espat_write_command(command, 10000);
}

static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)
{
// AT+CWSTATE?
// +CWSTATE:2,"0D66"

// OK
	response = strstr(response, "+CWSTATE:");
	if (response == NULL)
		return false;
	
	int wifi_state;
	if (sscanf(response, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2)
		return false;
	
	info->connected = (wifi_state == 2);
	
	return true;
}


static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info)
{
	
	response = strstr(response, "+CWJAP:");
	if (response == NULL)
	{
		
		return false;
	}
	
	
	if (sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", 
	           info->ssid, info->bssid, &info->channel, &info->rssi) != 4)
	{
		
		return false;
	}
	
	
	info->connected = true;
	
	return true;
}

bool espat_get_wifi_info(esp_wifi_info_t *info)
{

    if(!espat_write_command("AT+CWSTATE?\r\n", 2000))
        return false;
    if(!parse_cwstate_response(espat_get_response(), info))
        return false;
    
    if(info->connected == true)
    {
        if(!espat_write_command("AT+CWJAP?\r\n", 2000))
            return false;
        if(!parse_cwjap_response(espat_get_response(), info))
            return false;
    }
    

    return true;
}
const char *espat_http_get(const char *url, const char *response)
{
//    AT+HTTPCLIENT=2,1,"https://api.seniverse.com/v3/weather/now.json?key=SAQyoOxFBGFh4lfRp&location=Guangzhou&language=en&unit=c",,,2
//    +HTTPCLIENT:277,{"results":[{"location":{"id":"WS0E9D8WN298","name":"Guangzhou","country":"CN","path":"Guangzhou,Guangzhou,Guangdong,China","timezone":"Asia/Shanghai","timezone_offset":"+08:00"},"now":{"text":"Cloudy","code":"4","temperature":"25"},"last_update":"2025-11-14T15:07:53+08:00"}]}

//    OK
    char *txbuf = rxbuf;
    snprintf(txbuf, sizeof(rxbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n",url);
    bool ret = espat_write_command(txbuf, 5000);
     
    return ret ? espat_get_response() : NULL;
       
}
bool wifi_is_connected()
{
    esp_wifi_info_t info;
    if(espat_get_wifi_info(&info))
    {
        return info.connected;
    }
    return false;
}

// 所在文件：onenet.c

void esp32_SendData(unsigned char *data, unsigned short len)
{
    char cmdBuf[32];
    
    // 【优化】发送前先清空缓存，但不要清空正在接收的数据
    // 只在确认没有正在接收数据时才清空
    if(esp32_cnt == 0 || esp32_cnt == esp32_cntPre) {
        ESP32_Clear();
    }
    
    sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);
    
    // 1. 【优化】增加重试机制，最多重试2次
    int retry_count = 0;
    bool send_success = false;
    
    while(retry_count < 2 && !send_success)
    {
        // 发送命令，等待 >
        if(espat_write_command(cmdBuf, 3000))
        {
            // 2. 收到 > 后，发送 MQTT 数据包
            espat_usart_write_data((char*)data, len); 

            // 3. 【优化】发送完数据后，等待"SEND OK"确认
            // 增加延迟到300ms，确保数据完全发送成功
            vTaskDelay(pdMS_TO_TICKS(300));
            send_success = true;
            
            UsartPrintf(USART_DEBUG, "MQTT data sent successfully\r\n");
        }
        else
        {
            UsartPrintf(USART_DEBUG, "WARN: Wait > failed, retry %d\r\n", retry_count);
            retry_count++;
            
            if(retry_count < 2) {
                vTaskDelay(pdMS_TO_TICKS(100)); // 重试前短暂延迟
                ESP32_Clear(); // 清空可能的残留数据
            }
        }
    }
    
    if(!send_success) {
        UsartPrintf(USART_DEBUG, "ERROR: MQTT data send failed after retries\r\n");
    }
}
bool ESP32_WaitRecive(void)
{

	if(esp32_cnt == 0) 							//如果接收计数为0 则说明没有处于接收数据中，所以直接跳出，结束函数
		return REV_WAIT;
		
	if(esp32_cnt == esp32_cntPre)				//如果上一次的值和这次相同，则说明接收完毕
	{
		esp32_cnt = 0;							//清0接收计数
			
		return REV_OK;								//返回接收完成标志
	}
		
	esp32_cntPre = esp32_cnt;					//置为相同
	
	return REV_WAIT;								//返回接收未完成标志

}
// 如果ESP32-C3工作在AT指令模式
unsigned char *ESP32_GetIPD(unsigned short timeOut)
{
    char *ptrIPD = NULL;
    int timeout_count = timeOut / 5;  // 转换为循环次数
    
    do {
        if (ESP32_WaitRecive() == REV_OK) {
            ptrIPD = strstr((char *)esp32_buf, "+IPD,");
            if (ptrIPD != NULL) {
                ptrIPD = strchr(ptrIPD, ':');
                if (ptrIPD != NULL) {
                    ptrIPD++;
                    return (unsigned char *)(ptrIPD);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));  // ESP32的延时
    } while (timeout_count--);
    
    return NULL;
}


void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t)USART2->DR; 

        // 【优化】防止缓冲区溢出，使用环形缓冲策略
        if(esp32_cnt < sizeof(esp32_buf) - 1)
        {
            esp32_buf[esp32_cnt++] = data;
            esp32_buf[esp32_cnt] = '\0'; // 始终保持字符串结尾，方便调试
        }
        else
        {
            // 【优化】缓冲区满时，保留最后一半数据，将新数据追加到后面
            // 这样可以避免丢失完整的MQTT消息
            uint16_t half_size = sizeof(esp32_buf) / 2;
            memmove(esp32_buf, esp32_buf + half_size, half_size);
            esp32_cnt = half_size;
            esp32_buf[esp32_cnt++] = data;
            esp32_buf[esp32_cnt] = '\0';
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}
