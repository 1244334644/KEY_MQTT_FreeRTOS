#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "stm32f4xx.h"
#include "aht20_desc.h"
#include "aht20.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tim_delay.h"



//I2C1  CLK->PB6
//I2C1  SDA->PB7

static bool aht20_is_ready(aht20_desc_t aht20);
static bool aht20_write(aht20_desc_t aht20, uint8_t data[], uint32_t len);
static bool aht20_read(aht20_desc_t aht20, uint8_t data[], uint32_t len);
static bool aht20_read_status(aht20_desc_t aht20, uint8_t* status);

bool aht20_Init(aht20_desc_t aht20)
{
	if(aht20 == NULL)
	{
		return false;
	}
	I2C_InitTypeDef I2C_InitStructure;
	I2C_StructInit(&I2C_InitStructure);
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 100ul*1000ul;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_OwnAddress1 = 0x00;
	I2C_Init(aht20->I2C, &I2C_InitStructure);
	I2C_Cmd(aht20->I2C, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = aht20->SCLPin | aht20->SDAPin;
	GPIO_Init(aht20->GPort, &GPIO_InitStructure);
	GPIO_PinAFConfig(aht20->GPort, aht20->SCLPinsource, GPIO_AF_I2C1);
	GPIO_PinAFConfig(aht20->GPort, aht20->SDAPinsource, GPIO_AF_I2C1);
	

	vTaskDelay(pdMS_TO_TICKS(40));
	if(aht20_is_ready(aht20))
		return true;
	if(aht20_write(aht20, (uint8_t[]){0xBE, 0x08, 0x00}, 3) == false)
	{
		return false;
	}
	for(uint32_t i=0; i<20; i++)
	{
		vTaskDelay(pdMS_TO_TICKS(5));
		if(aht20_is_ready(aht20))
		{
			return true;
		}
		
	}

	return false;

}

#define I2C_CHECK_EVENT(EVENT, TIMEOUT) \
	do {\
		uint32_t timeout = TIMEOUT;\
		while (!I2C_CheckEvent(I2C1, EVENT) && timeout>0) \
		{ \
			tim_delay_us(10); \
			timeout-=10; \
		}\
		if(timeout<=0) return false;\
	}while(0)


static bool aht20_write(aht20_desc_t aht20, uint8_t data[], uint32_t len)
{
	I2C_AcknowledgeConfig(aht20->I2C, ENABLE);
	I2C_GenerateSTART(aht20->I2C, ENABLE);
	I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
	I2C_Send7bitAddress(aht20->I2C, 0x70, I2C_Direction_Transmitter);
	I2C_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 1000);
	for(uint32_t i=0; i<len; i++)
	{
		I2C_SendData(aht20->I2C, data[i]);
		I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
	}

	I2C_GenerateSTOP(aht20->I2C, ENABLE);

	return true;
}


static bool aht20_read(aht20_desc_t aht20, uint8_t data[], uint32_t len)
{

	I2C_AcknowledgeConfig(aht20->I2C, ENABLE);
	I2C_GenerateSTART(aht20->I2C, ENABLE);
	I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
	
	I2C_Send7bitAddress(aht20->I2C, 0x70, I2C_Direction_Receiver);
	I2C_CHECK_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 1000);
	
	for(uint32_t i=0; i<len; i++)
	{  
		if(i == len-1)
		{
			I2C_AcknowledgeConfig(aht20->I2C, DISABLE);
		}
	    I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED, 1000);
	    data[i] = I2C_ReceiveData(aht20->I2C);
	}
	
	// 8. 结束传输
	I2C_GenerateSTOP(aht20->I2C, ENABLE);
	
	return true;

}

static bool aht20_read_status(aht20_desc_t aht20, uint8_t* status)
{
	uint8_t cmd=0x71;
	if(aht20_write(aht20, &cmd, 1) == false)
	{
		return false;
	}
	if(aht20_read(aht20, status, 1) == false)
	{
		return false;
	}
    return true;
}

static bool aht20_is_busy(aht20_desc_t aht20)
{
	uint8_t status;
	if(aht20_read_status(aht20, &status) == false)
	{
		return false;
	}
	return (status & 0x80) != 0;
}

static bool aht20_is_ready(aht20_desc_t aht20)
{
	uint8_t status;
	if(aht20_read_status(aht20, &status) == false)
	{
		return false;
	}
	return (status & 0x08) != 0;
}

bool aht20_start_measure(aht20_desc_t aht20)
{
	return aht20_write(aht20, (uint8_t[]){0xAC, 0x33, 0x00}, 3);
}

bool aht20_wait_for_measure(aht20_desc_t aht20)
{
	for(uint32_t i=0; i<20; i++)
	{
		vTaskDelay(pdMS_TO_TICKS(10));
		if(aht20_is_busy(aht20) == false)
		{
			return true;
		}
		
	}
	return false;
}

bool aht20_read_measurement(aht20_desc_t aht20, float *temp, float *humidity)
{
	uint8_t data[6];
	if(aht20_read(aht20, data, 6) == false)	
	{
		return false;
	}
	uint32_t raw_humidity = (uint32_t)(data[1] << 12) | 
							(uint32_t)(data[2] << 4) | 
							(uint32_t)(data[3]&0xf0) >> 4;
	uint32_t raw_temperature = (uint32_t)(data[3] & 0x0F) << 16 | 
								(uint32_t)(data[4] << 8) | 
								(uint32_t)(data[5]);
	*humidity = raw_humidity * 100.0f / (float)0x100000 ;
	*temp = raw_temperature * 200.0f / (float)0x100000 - 50.0f;
	return true;
}

