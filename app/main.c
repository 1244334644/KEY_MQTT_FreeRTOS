#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "board.h"
#include "wifi_connect.h"
#include "loop.h"
#include "work_queue.h"  // 添加工作队列头文件
#include "ui.h"
#include "LCD.h"
#include "page.h"


static void main_init(void *param)
{
	board_init();
	ui_init(lcd);

    welcome_display(lcd);
	wireless_init();
    wifi_page_display(lcd);
    wireless_wait_connect();
    printf("System Starting...\n");
    
	main_page_display(lcd);

	// 【关键】必须在 main_loop_init() 之前初始化工作队列
	workqueue_init();
	
	main_loop_init();
	
	vTaskDelete(NULL);

}


int main(void)
{
    
    // 4. 启动调度器
    xTaskCreate(main_init, "init", 1024, NULL, 9, NULL);
    vTaskStartScheduler();
   
    while (1)
    {
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
