#ifndef __LCD_DESC_H__
#define __LCD_DESC_H__

#include "stm32f4xx.h"

struct lcd_desc
{
    SPI_TypeDef *SPI;
    GPIO_TypeDef* Port;
    uint32_t RSTPin;
    uint32_t DCPin;
    uint32_t CSPin;
    uint32_t BLPin;
    uint32_t SCKPin;
    uint32_t MOSIPin;
    uint16_t SCKPinsource;
    uint16_t MOSIPinsource;
    uint16_t CSPinsource;

};



#endif // !__LCD_DESC_H__
