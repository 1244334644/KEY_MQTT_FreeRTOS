#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stm32f4xx.h"
#include "led.h"
#include "led_desc.h"



void led_init(led_desc_t led)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_InitStructure.GPIO_Pin = led->Pin;
    GPIO_Init(led->Port, &GPIO_InitStructure);
    GPIO_SetBits(led->Port, led->Pin);
}

void led_on(led_desc_t led)
{
    GPIO_WriteBit(led->Port, led->Pin, led->OnBit);
}

void led_off(led_desc_t led)
{
    GPIO_WriteBit(led->Port, led->Pin, led->OffBit);
}

void led_toggle(led_desc_t led)
{
    GPIO_ToggleBits(led->Port, led->Pin);
}

