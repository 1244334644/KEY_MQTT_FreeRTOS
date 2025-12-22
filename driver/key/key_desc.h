#ifndef __KEY_DESC_H__
#define __KEY_DESC_H__


#include "stm32f4xx.h"
#include <stdbool.h>
struct key_desc
{
    GPIO_TypeDef* Port;
    uint32_t Pin;
    bool State;
};



#endif//__KEY_DESC_H__
