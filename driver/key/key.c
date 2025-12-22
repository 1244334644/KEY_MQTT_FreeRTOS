#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stm32f4xx.h"
#include "key.h"
#include "key_desc.h"



void key_init(key_desc_t key)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

    GPIO_InitStructure.GPIO_Pin = key->Pin;
    GPIO_Init(key->Port, &GPIO_InitStructure);
}

bool key_read(key_desc_t key)
{
    key->State = GPIO_ReadInputDataBit(key->Port, key->Pin);
    return key->State;
}
