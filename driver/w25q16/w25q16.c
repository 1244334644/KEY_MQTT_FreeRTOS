#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "w25q16.h"
#include "w25q16_desc.h"


static volatile bool dma_transfer_complete = false;

static void w25q16_io_init(w25q16_desc_t w25q16)
{
	if (w25q16 == NULL)
	{
		return;
	}
	GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = w25q16->SCK_Pin | w25q16->MISO_Pin | w25q16->MOSI_Pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(w25q16->GPort, &GPIO_InitStructure);

	GPIO_PinAFConfig(w25q16->GPort,  w25q16->SCK_Pinsource, GPIO_AF_SPI1);
	GPIO_PinAFConfig(w25q16->GPort,  w25q16->MISO_Pinsource, GPIO_AF_SPI1);
	GPIO_PinAFConfig(w25q16->GPort,  w25q16->MOSI_Pinsource, GPIO_AF_SPI1);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Pin =  w25q16->CS_Pin;
	GPIO_Init(w25q16->GPort, &GPIO_InitStructure);
	
}

static void w25q16_spi_init(w25q16_desc_t w25q16)
{

    SPI_InitTypeDef SPI_InitStructure;
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    // SPI_InitStructure.SPI_CRCPolynomial = 0;
    SPI_Init(w25q16->SPI, &SPI_InitStructure);
    SPI_I2S_DMACmd(w25q16->SPI, SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx, ENABLE);
    SPI_Cmd(w25q16->SPI, ENABLE);

}

// 预配置 DMA（只配置不变的参数）
static void w25q16_dma_init(w25q16_desc_t w25q16)
{
    // RX DMA 预配置
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_3;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DR;
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStruct.DMA_Memory0BaseAddr = 0;  // 占位
    DMA_InitStruct.DMA_BufferSize = 1;       // 占位
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_Init(DMA2_Stream0, &DMA_InitStruct);
    
    // TX DMA 预配置
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_Init(DMA2_Stream3, &DMA_InitStruct);
}
// DMA 传输函数
// dir: 0-读取(RX), 1-写入(TX)
static void dma_transfer(uint8_t *pTxData, uint8_t *pRxData, uint32_t len)
{
    if(len == 0) return;
    
    static uint8_t tx_dummy = 0xFF;
    static uint8_t rx_dummy;
    
    // 等待上次完成
    while(DMA_GetCmdStatus(DMA2_Stream3) != DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream0) != DISABLE);
    
    // 清除标志
    DMA2->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;
    DMA2->LIFCR = DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3;
    
    // 只更新必要的寄存器（直接操作寄存器，更快）
    // RX
    if(pRxData != NULL) {
        DMA2_Stream0->M0AR = (uint32_t)pRxData;
        DMA2_Stream0->CR |= DMA_SxCR_MINC;   // 内存递增
    } else {
        DMA2_Stream0->M0AR = (uint32_t)&rx_dummy;
        DMA2_Stream0->CR &= ~DMA_SxCR_MINC;  // 内存不递增
    }
    DMA2_Stream0->NDTR = len;
    
    // TX
    if(pTxData != NULL) {
        DMA2_Stream3->M0AR = (uint32_t)pTxData;
        DMA2_Stream3->CR |= DMA_SxCR_MINC;
    } else {
        DMA2_Stream3->M0AR = (uint32_t)&tx_dummy;
        DMA2_Stream3->CR &= ~DMA_SxCR_MINC;
    }
    DMA2_Stream3->NDTR = len;
    
    // 启用中断
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;
    
    dma_transfer_complete = false;
    
    // 启动 DMA
    DMA2_Stream0->CR |= DMA_SxCR_EN;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
    
    // 等待完成
    while(!dma_transfer_complete);
    
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);
}


static void w25q16_int_init(w25q16_desc_t w25q16)
{

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void w25q16_init(w25q16_desc_t w25q16)
{
    w25q16_io_init(w25q16);
    w25q16_spi_init(w25q16);
    w25q16_dma_init(w25q16);
	w25q16_int_init(w25q16);

}

// 等待 W25Q16 空闲
void w25q16_wait_busy(w25q16_desc_t w25q16)
{
    uint8_t cmd = 0x05;
    uint8_t status = 0;
    
    do {
        GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
        dma_transfer(&cmd, NULL, 1);      // 发送读状态命令
        dma_transfer(NULL, &status, 1);   // 读取状态
        GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
    } while(status & 0x01);  // BUSY 位为 1 时继续等待
}

// 写使能
void w25q16_write_enable(w25q16_desc_t w25q16)
{
    uint8_t cmd = 0x06;
    GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
    dma_transfer(&cmd, NULL, 1);
    GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
}
// 扇区擦除 (4KB)
void w25q16_erase_sector(w25q16_desc_t w25q16, uint32_t addr)
{
    uint8_t cmd[4];
    
    w25q16_write_enable(w25q16);
    
    cmd[0] = 0x20;  // Sector Erase
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;
    
    GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
    dma_transfer(cmd, NULL, 4);
    GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
    
    w25q16_wait_busy(w25q16);
}

// 块擦除 (64KB)
void w25q16_erase_block(w25q16_desc_t w25q16, uint32_t addr)
{
    uint8_t cmd[4];
    
    w25q16_write_enable(w25q16);
    
    cmd[0] = 0xD8;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;
    
    GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
    dma_transfer(cmd, NULL, 4);
    GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
    
    w25q16_wait_busy(w25q16);
}

// 读取任意长度数据
void w25q16_read_data(w25q16_desc_t w25q16, uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t cmd[4];
    cmd[0] = 0x03;  // Read Data
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;
    
    GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
    dma_transfer(cmd, NULL, 4);
    dma_transfer(NULL, buf, len);
    GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
}

// 写入数据 (自动处理页边界，每页256字节)
void w25q16_write_data(w25q16_desc_t w25q16, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t cmd[4];
    uint32_t page_remain;
    
    while(len > 0) {
        // 计算当前页剩余空间
        page_remain = 256 - (addr % 256);
        if(page_remain > len) page_remain = len;
        
        // 写使能
        w25q16_write_enable(w25q16);
        
        // 页编程
        cmd[0] = 0x02;
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        
        GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
        dma_transfer(cmd, NULL, 4);
        dma_transfer((uint8_t*)buf, NULL, page_remain);
        GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
        
        w25q16_wait_busy(w25q16);
        // 更新地址和指针
        addr += page_remain;
        buf += page_remain;
        len -= page_remain;
    }
}

void w25q16_SaveByte(w25q16_desc_t w25q16, const uint8_t *data, uint16_t len)
{
    if (w25q16 == NULL) return;
    
    // 1. 写使能
    w25q16_write_enable(w25q16);
    
	// 2. 扇区擦除 (地址 0x000000)
    // 3. 等待擦除完成
    w25q16_erase_sector(w25q16, 0x000000);

    // 4. 写使能
    w25q16_write_enable(w25q16);
    
    // 5. 页编程
    w25q16_write_data(w25q16, 0x000000, data, len);

    // 6. 等待写入完成
    w25q16_wait_busy(w25q16);
}

uint8_t w25q16_LoadByte(w25q16_desc_t w25q16)
{
    uint8_t cmd[4] = {0x03, 0x00, 0x00, 0x00};
    uint8_t data = 0;
    
    GPIO_ResetBits(w25q16->GPort, w25q16->CS_Pin);
    dma_transfer(cmd, NULL, 4);      // 发送读命令和地址
    dma_transfer(NULL, &data, 1);    // 读取数据
    GPIO_SetBits(w25q16->GPort, w25q16->CS_Pin);
    
    return data;
}



// 使用 RX DMA 中断 (Stream0)
void DMA2_Stream0_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0))
    {
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);
        dma_transfer_complete = true;
    }
}

