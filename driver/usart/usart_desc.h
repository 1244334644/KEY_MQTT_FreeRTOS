#ifndef __USART_DESC_H__
#define __USART_DESC_H__
#include "stm32f4xx.h"

struct usart_desc
{
    USART_TypeDef* Port;
    GPIO_TypeDef* GPort;
    uint32_t TxPin;
    uint32_t RxPin;
    IRQn_Type IRQn;
};


#endif // !__USART_DESC_H__
