#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "led.h"
#include "led_desc.h"
#include "key.h"
#include "key_desc.h"
#include "usart.h"
#include "usart_desc.h"
#include "aht20.h"
#include "aht20_desc.h"
#include "tim_delay.h"
#include "LCD.h"
#include "LCD_desc.h"
#include "rtc.h"


static struct led_desc led1_desc = 
{
	.Port = GPIOA,
	.Pin = GPIO_Pin_6,
	.OnBit = Bit_RESET,
	.OffBit = Bit_SET,
};
led_desc_t led1 = &led1_desc;

static struct led_desc led2_desc = 
{
	.Port = GPIOA,
	.Pin = GPIO_Pin_7,
	.OnBit = Bit_RESET,
	.OffBit = Bit_SET,
};
led_desc_t led2 = &led2_desc;

static struct key_desc key1_desc = 
{
	.Port = GPIOE,
	.Pin = GPIO_Pin_4,
	.State = true,
};
key_desc_t key1 = &key1_desc;

static struct key_desc key2_desc = 
{
	.Port = GPIOE,
	.Pin = GPIO_Pin_3,
	.State = true,
};
key_desc_t key2 = &key2_desc;

static struct usart_desc usart1_desc = 
{
	.Port = USART1,
	.GPort = GPIOA,
	.TxPin = GPIO_Pin_9,
	.RxPin = GPIO_Pin_10,
	.IRQn =  USART1_IRQn,
};
usart_desc_t usart1 = &usart1_desc;

static struct usart_desc usart2_desc = 
{
	.Port = USART2,
	.GPort = GPIOA,
	.TxPin = GPIO_Pin_2,
	.RxPin = GPIO_Pin_3,
	.IRQn =  USART2_IRQn,
};
usart_desc_t usart2 = &usart2_desc;

static struct aht20_desc aht20_desc = 
{
	.I2C = I2C1,
	.GPort = GPIOB,
	.SCLPin = GPIO_Pin_6,
	.SDAPin = GPIO_Pin_7,
	.SCLPinsource = GPIO_PinSource6,
	.SDAPinsource = GPIO_PinSource7,
};
aht20_desc_t aht20 = &aht20_desc;

static struct lcd_desc lcd_desc = 
{
	.SPI = SPI2,
	.Port = GPIOB,
	.RSTPin = GPIO_Pin_9,
	.DCPin = GPIO_Pin_10,
	.CSPin = GPIO_Pin_12,
	.BLPin = GPIO_Pin_0,
	.SCKPin = GPIO_Pin_13,
	.MOSIPin = GPIO_Pin_15,
	.SCKPinsource = GPIO_PinSource13,
	.MOSIPinsource = GPIO_PinSource15,
	.CSPinsource = GPIO_PinSource10,
};
lcd_desc_t lcd = &lcd_desc;


void board_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);	//中断控制器分组设置
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE); 
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
	RCC_LSEConfig(RCC_LSE_ON);
	while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);

	tim_delay_init();
	rtc_init();
    led_init(led1);
    led_init(led2);
    key_init(key1);
    key_init(key2);
    usart_init(usart1);
	aht20_Init(aht20);
	lcd_init(lcd);
	
}


