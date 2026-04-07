#ifndef __W25Q16_DESC_H__
#define __W25Q16_DESC_H__
#include "stm32f4xx.h"

struct w25q16_desc
{
    SPI_TypeDef* SPI;
    
    GPIO_TypeDef* GPort;
    uint32_t SCK_Pin;
    uint32_t MISO_Pin;
    uint32_t MOSI_Pin;
    uint32_t CS_Pin;
    
    uint16_t SCK_Pinsource;
    uint16_t MOSI_Pinsource;
    uint16_t MISO_Pinsource;
    uint16_t CS_Pinsource;

};


#endif // !__W25Q16_DESC_H__
