#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "usart_desc.h"
void usart_init(usart_desc_t usart)
{
	if (usart == NULL)
	{
		return;
	}

	USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);

	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_Mode = USART_Mode_Tx|USART_Mode_Rx;
	USART_Init(usart->Port, &USART_InitStructure);
	
	GPIO_PinAFConfig(usart->GPort,  GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(usart->GPort,  GPIO_PinSource10, GPIO_AF_USART1);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = usart->TxPin | usart->RxPin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(usart->GPort, &GPIO_InitStructure);

	USART_Cmd(usart->Port, ENABLE);

	USART_ITConfig(usart->Port, USART_IT_RXNE, ENABLE);									//使能接收中断
	
	NVIC_InitTypeDef nvic_initstruct;
	nvic_initstruct.NVIC_IRQChannel = usart->IRQn;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 2;
	NVIC_Init(&nvic_initstruct);
}

void usart_write(usart_desc_t usart, char str[])
{
	int len = strlen(str);
	for (int i = 0; i<len; i++)
	{
		USART_ClearFlag(usart->Port, USART_FLAG_TC);
		USART_SendData(usart->Port, (uint16_t)str[i]);
		while (USART_GetFlagStatus(usart->Port, USART_FLAG_TC) == RESET);
		
	}
}

int usart_read(usart_desc_t usart)
{

	USART_ClearFlag(usart->Port, USART_FLAG_RXNE);
	while (USART_GetFlagStatus(usart->Port, USART_FLAG_RXNE) == RESET);
	uint8_t ch = (uint8_t)USART_ReceiveData(usart->Port);
	return ch;

}

void UsartPrintf(USART_TypeDef *USARTx, char *fmt,...)
{

	unsigned char UsartPrintfBuf[296];
	va_list ap;
	unsigned char *pStr = UsartPrintfBuf;
	
	va_start(ap, fmt);
	vsnprintf((char *)UsartPrintfBuf, sizeof(UsartPrintfBuf), fmt, ap);							//格式化
	va_end(ap);
	
	while(*pStr != 0)
	{
		USART_SendData(USARTx, *pStr++);
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
	}

}

void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len)
{

	unsigned short count = 0;
	
	for(; count < len; count++)
	{
		USART_SendData(USARTx, *str++);									//发送数据
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);		//等待发送完成
	}

}

int fputc(int ch, FILE *f)
{
	(void)f;

	USART_ClearFlag(USART1, USART_FLAG_TC);
	USART_SendData(USART1, (uint16_t)ch);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

	return ch;

}



void USART1_IRQHandler(void)
{

	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) //接收中断
	{
		USART_ClearFlag(USART1, USART_FLAG_RXNE);
	}

}