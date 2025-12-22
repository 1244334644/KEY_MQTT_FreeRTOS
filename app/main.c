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
#include "onenet.h"  // 引入OneNet业务逻辑
#include "wifi_connect.h"


// ================= 全局变量 =================
// 这些变量在 onenet.c 中被 extern 引用，用于模拟传感器数据
bool led1_state, led2_state, key1_state, key2_state;

// 【新增】状态改变标志
extern bool state_changed;

void key_task(void *pvParameters);
void mqtt_task(void *pvParameters); // 新增 MQTT 任务

QueueHandle_t xQueue = NULL;

int main(void)
{

    // 1. 硬件初始化
    board_init();
    
    // 2. 创建队列 (存储按键事件)
    // 深度10，每个单元存储一个 uint16_t (代表按键ID)
    xQueue = xQueueCreate(10, sizeof(uint16_t));

    // 3. 创建任务
    // 按键任务：负责扫描输入
    xTaskCreate(key_task, "key1", 256, key1, 3, NULL);
    xTaskCreate(key_task, "key2", 256, key2, 3, NULL);
    
    // MQTT任务：负责网络通信 (堆栈设大一点，因为涉及到JSON解析和字符串处理)
    xTaskCreate(mqtt_task, "mqtt", 1024, NULL, 4, NULL);

    // 4. 启动调度器
    printf("System Starting...\n");
    vTaskStartScheduler();

    while (1)
    {
    }
}

// ==========================================
// 任务：MQTT 通信主任务
// ==========================================
static void mqtt_task(void *pvParameters)
{
    wireless_init();
    wireless_wait_connect();

    unsigned char *dataPtr = NULL;
    
    // 4. 连接 OneNet
    if(OneNet_DevLink() == 0)
    {
        printf("OneNet Connected.\n");
        vTaskDelay(pdMS_TO_TICKS(500)); 
        OneNET_Subscribe(); 
    }
    else
    {
        printf("OneNet Connect Failed.\n");
    }
    OneNET_Subscribe();
    
    uint32_t last_send_time = 0;
    uint32_t min_send_interval = 300;  // 【关键优化】最小发送间隔300ms,避免发送过快
    
    while(1)
    {		
        uint32_t current_time = xTaskGetTickCount();
        
        // 1. 高频检查接收数据（优先级最高）
        //    【优化】超时时间200ms，快速响应网页端控制命令
        dataPtr = ESP32_GetIPD(200);
        if(dataPtr != NULL)
        {
            printf("Received data from OneNet!\n");
            OneNet_RevPro(dataPtr);
            
            // 【优化】接收到云端指令后，延迟处理并上报
            // 增加延迟确保LED操作完成，避免发送冲突
            vTaskDelay(pdMS_TO_TICKS(150));
            
            // 检查距离上次发送是否足够长
            if((current_time - last_send_time) >= pdMS_TO_TICKS(min_send_interval))
            {
                OneNet_SendData();
                last_send_time = xTaskGetTickCount();  // 更新为实际发送时间
                printf("Cloud command processed and state reported\n");
            }
            else
            {
                printf("Skip report: too soon after last send\n");
                // 标记需要稍后上报
                state_changed = true;
            }
        }
        
        // 2. 【关键优化】检查本地状态改变标志，但增加防抖
        if(state_changed)
        {
            // 确保距离上次发送至少300ms，避免发送过快
            if((current_time - last_send_time) >= pdMS_TO_TICKS(min_send_interval))
            {
                printf("Local state changed, reporting...\n");
                OneNet_SendData();
                state_changed = false;  // 清除标志
                last_send_time = xTaskGetTickCount();  // 更新发送时间
            }
            else
            {
                // 距离上次发送太近，等待下次循环再尝试
                uint32_t wait_time = min_send_interval - (current_time - last_send_time);
                printf("State changed but waiting %dms before send\n", wait_time);
                vTaskDelay(pdMS_TO_TICKS(wait_time + 50));  // 等待足够时间
            }
        }
        
        // 3. 定期上报 - 【优化】改为15秒一次，减少发送频率
        else if((current_time - last_send_time) >= pdMS_TO_TICKS(15000))
        {
            printf("Periodic report (every 15s)\n");
            OneNet_SendData();
            last_send_time = xTaskGetTickCount();
        }
        
        // 4. 循环延时
        vTaskDelay(pdMS_TO_TICKS(50));  // 【优化】从20ms改为50ms，减少CPU占用
    }
}

// ==========================================
// 任务：按键扫描
// ==========================================
static void key_task(void *pvParameters)
{
    key_desc_t key = (key_desc_t)pvParameters;
    uint16_t key_id = 0;

    // 根据传入的参数区分是 Key1 还是 Key2
    if(key == key1) key_id = 1;
    if(key == key2) key_id = 2;

    while (1)
    {
        // 按键按下 (低电平有效)
        if(key_read(key) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if(key_read(key) == 0)
            {
                // 发送消息到队列
                xQueueSend(xQueue, &key_id, 0);
                switch (key_id)
                {
                    case 1:
                        key1_state = 1;
                        led_toggle(led1);
                        led1_state = !led1_state;
                        state_changed = true;  // 【关键】设置状态改变标志
                        printf("Key1 pressed, LED1=%d, state_changed=true\n", led1_state);
                        break;
                    case 2:
                        key2_state = 1;
                        led_toggle(led2);
                        led2_state = !led2_state;
                        state_changed = true;  // 【关键】设置状态改变标志
                        printf("Key2 pressed, LED2=%d, state_changed=true\n", led2_state);
                        break;
                    default:
                        break;
                }
                
                // 等待按键释放
                while(key_read(key) == 0)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                key1_state = 0;
                key2_state = 0;
                printf("Key%d Released\n", key_id);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ==========================================
// FreeRTOS Hooks (保持不变)
// ==========================================
void vAssertCalled(const char *file, int line)
{
    portDISABLE_INTERRUPTS();
    printf("Assert Called: %s(%d)\n", file, line);
    while(1);
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack Overflowed: %s\n", pcTaskName);
    configASSERT(0);
}

void vApplicationMallocFailedHook( void )
{
    printf("Malloc Failed\n");
    configASSERT(0);
}
