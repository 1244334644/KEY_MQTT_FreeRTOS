#ifndef __USART_H__
#define __USART_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx.h"

struct usart_desc;

typedef struct usart_desc *usart_desc_t;
#define USART_DEBUG		USART1		//调试打印所使用的串口组

void usart_init(usart_desc_t usart);
int usart_read(usart_desc_t usart);
void usart_write(usart_desc_t usart, char str[]);
void UsartPrintf(USART_TypeDef *USARTx, char *fmt,...);
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len);


#endif // !__USART_H__
