#ifndef __AHT20_DESC_H__
#define __AHT20_DESC_H__
#include "stm32f4xx.h"

struct aht20_desc
{
    I2C_TypeDef* I2C;
    GPIO_TypeDef* GPort;
    uint32_t SCLPin;
    uint32_t SDAPin;
    uint32_t SCLPinsource;
    uint32_t SDAPinsource;
};


#endif // !__AHT20_DESC_H__
