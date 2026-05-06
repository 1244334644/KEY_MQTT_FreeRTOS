#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "LCD.h"
#include "LCD_desc.h"
#include "w25q16.h"
#include "w25q16_desc.h"
#include "w25q16_burn.h"

#include "font.h"
#include "img.h"

// SCK-> PB13 SPI2_SCK 时钟线（硬件SPI固定）
// SDA-> PB15 SPI2_MOSI 数据线（硬件SPI固定）
// CS-> PB12 SPI2_NSS 片选线（可软件控制）
// DC-> PB10 GPIO输出 数据/命令控制
// RST-> PB9 GPIO输出 复位控制
// BL-> PB0 GPIO输出 背光控制（PWM可选）

#define LCD_HEIGHT 320
#define LCD_WIDTH 240
#define LCD_COLUMN_OFFSET 0
#define SPI2_MISO_PIN GPIO_Pin_14 

static SemaphoreHandle_t write_gram_semaphore; 
// static bool lcd_read(lcd_desc_t lcd, uint8_t* data, uint16_t len);

static void st7789_init(lcd_desc_t lcd);
static void st7789_write_register(lcd_desc_t lcd,uint8_t reg, uint8_t data[], uint16_t length);
void st7789_fill_color(lcd_desc_t lcd, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

static void st7789_io_init(lcd_desc_t lcd)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    GPIO_SetBits(lcd->Port, lcd->RSTPin | lcd->DCPin | lcd->BLPin | lcd->CSPin);
    GPIO_ResetBits(lcd->Port, lcd->BLPin);
   
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Pin =  lcd->RSTPin | lcd->DCPin | lcd->BLPin | lcd->CSPin;
	GPIO_Init(lcd->Port, &GPIO_InitStructure);

    GPIO_PinAFConfig(lcd->Port, lcd->SCKPinsource, GPIO_AF_SPI2);
    GPIO_PinAFConfig(lcd->Port, lcd->MOSIPinsource, GPIO_AF_SPI2);
    GPIO_PinAFConfig(lcd->Port, GPIO_PinSource14, GPIO_AF_SPI2);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Pin =   lcd->SCKPin | lcd->MOSIPin | SPI2_MISO_PIN;
    GPIO_Init(lcd->Port, &GPIO_InitStructure);

}

static void st7789_spi_init(lcd_desc_t lcd)
{

    SPI_InitTypeDef SPI_InitStructure;
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    // SPI_InitStructure.SPI_CRCPolynomial = 0;
    SPI_Init(lcd->SPI, &SPI_InitStructure);
    SPI_DMACmd(lcd->SPI, SPI_DMAReq_Tx, ENABLE);
    SPI_Cmd(lcd->SPI, ENABLE);

}

static void st7789_dma_init(lcd_desc_t lcd)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_0;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);
    DMA_Init(DMA1_Stream4, &DMA_InitStruct);

}

static void st7789_int_init(lcd_desc_t lcd)
{

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA1_Stream4_IRQn, 5);
}

void lcd_init(lcd_desc_t lcd)
{
    write_gram_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_gram_semaphore);

    st7789_spi_init(lcd);
    st7789_dma_init(lcd);
    st7789_int_init(lcd);
    st7789_io_init(lcd);
    
	st7789_init(lcd);
}

static void st7789_init(lcd_desc_t lcd)
{
	GPIO_ResetBits(lcd->Port, lcd->RSTPin);
    vTaskDelay(pdMS_TO_TICKS(2));
    GPIO_SetBits(lcd->Port, lcd->RSTPin);
    vTaskDelay(pdMS_TO_TICKS(120));

	st7789_write_register(lcd, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    

    st7789_write_register(lcd, 0x36, (uint8_t[]){0x00}, 1);
    st7789_write_register(lcd, 0x3A, (uint8_t[]){0x55}, 1);
	// 设置显示区域
    // st7789_write_register(lcd, 0x2A, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4); // Column: 0-239
    // st7789_write_register(lcd, 0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0x17}, 4); // Page: 0-319

    st7789_write_register(lcd, 0xB2, (uint8_t[]){0x0C,0x0C,0x00,0x33,0x33}, 5);
    st7789_write_register(lcd, 0xB7, (uint8_t[]){0x56}, 1);
	st7789_write_register(lcd, 0xBB, (uint8_t[]){0x18}, 1);
    st7789_write_register(lcd, 0xC0, (uint8_t[]){0x2C}, 1);
    st7789_write_register(lcd, 0xC2, (uint8_t[]){0x01}, 1);
	st7789_write_register(lcd, 0xC3, (uint8_t[]){0x1f}, 1);
    st7789_write_register(lcd, 0xC4, (uint8_t[]){0x20}, 1);
    st7789_write_register(lcd, 0xC6, (uint8_t[]){0x0F}, 1);
    st7789_write_register(lcd, 0xD0, (uint8_t[]){0xA6,0xA1}, 2);
 //   st7789_write_register(lcd, 0xD6, (uint8_t[]){0xA1}, 1);

    st7789_write_register(lcd, 0xE0, (uint8_t[]){0xD0,0x0D,0x14, 0x0B,0x0B,0x07, 0x3A,0x44,0x50, 0x08,0x13,0x13, 0x2D,0x32}, 14);
    st7789_write_register(lcd, 0xE1, (uint8_t[]){0xD0,0x0D,0x14, 0x0B,0x0B,0x07, 0x3A,0x44,0x50, 0x08,0x13,0x13, 0x2D,0x32}, 14);
    st7789_write_register(lcd, 0x21, NULL, 0);
    st7789_write_register(lcd, 0x29, NULL, 0);
    st7789_write_register(lcd, 0x2C, NULL, 0);

  	st7789_fill_color(lcd, 0, 0, LCD_WIDTH-1, LCD_HEIGHT-1 , 0x0000);

	GPIO_WriteBit(lcd->Port, lcd->BLPin, Bit_SET);

}
static bool in_screen_range(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    if (x1 >= LCD_WIDTH || y1 >= LCD_HEIGHT)
        return false;
    if (x2 >= LCD_WIDTH || y2 >= LCD_HEIGHT)
        return false;
    if (x1 > x2 || y1 > y2)
        return false;

    return true;
}

// x1 = 0 (二进制: 0000 0000 0000 0000)
// x2 = 239 (二进制: 0000 0000 1110 1111)

// 高字节和低字节分解：
// (x1 >> 8) & 0xff = 0x00  // 高字节
// x1 & 0xff = 0x00         // 低字节
// (x2 >> 8) & 0xff = 0x00  // 高字节  
// x2 & 0xff = 0xEF         // 低字节

// 最终发送的4个字节：0x00, 0x00, 0x00, 0xEF
static void st7789_set_range_and_prepare_gram(lcd_desc_t lcd,uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    st7789_write_register(lcd,0x2A, (uint8_t[]){(x1 >> 8) & 0xff, x1 & 0xff, (x2 >> 8) & 0xff, x2 & 0xff}, 4);//高8位和低8位分离
    st7789_write_register(lcd,0x2B, (uint8_t[]){(y1 >> 8) & 0xff, y1 & 0xff, (y2 >> 8) & 0xff, y2 & 0xff}, 4);
    st7789_write_register(lcd,0x2C, NULL, 0);
}





static void st7789_write_register(lcd_desc_t lcd,uint8_t reg, uint8_t data[], uint16_t length)
{

    // 【关键修复】切换模式前必须先关闭SPI，否则设置无效！
    SPI_Cmd(lcd->SPI, DISABLE);
    SPI_DataSizeConfig(lcd->SPI, SPI_DataSize_8b); 
    SPI_Cmd(lcd->SPI, ENABLE);

    GPIO_ResetBits(lcd->Port, lcd->CSPin);
    
    GPIO_ResetBits(lcd->Port, lcd->DCPin);
    SPI_SendData(lcd->SPI, reg);
    while (SPI_GetFlagStatus(lcd->SPI, SPI_FLAG_TXE) == RESET);
    while (SPI_GetFlagStatus(lcd->SPI, SPI_FLAG_BSY) != RESET);
    
    GPIO_SetBits(lcd->Port, lcd->DCPin);
    for (uint16_t i = 0; i < length; i++)
    {
        SPI_SendData(lcd->SPI, data[i]);
        while (!SPI_GetFlagStatus(lcd->SPI, SPI_FLAG_TXE));
    }
    while (SPI_GetFlagStatus(lcd->SPI, SPI_FLAG_BSY) != RESET);
    
    GPIO_SetBits(lcd->Port, lcd->CSPin);
}

static void st7789_write_gram(lcd_desc_t lcd, uint8_t data[], uint32_t length, bool spixel)
{
    // 1. SPI 位宽配置
    // 如果是单色填充，使用16位SPI(利用STM32硬件自动处理大小端)
    // 如果是图片，使用8位SPI(透传大端数据)
    if (spixel) {
        SPI_DataSizeConfig(lcd->SPI, SPI_DataSize_16b);
    } else {
        SPI_DataSizeConfig(lcd->SPI, SPI_DataSize_8b);
    }

    GPIO_ResetBits(lcd->Port, lcd->CSPin);
    GPIO_SetBits(lcd->Port, lcd->DCPin);

    uint32_t current_len = length; 
    uint8_t* current_data = data;

    // 预定义需要清除的位掩码：MSIZE(bit 13,14) | PSIZE(bit 11,12) | MINC(bit 10)
    // 0x6000 | 0x1800 | 0x0400 = 0x7C00
    const uint32_t CR_MASK_TO_CLEAR = DMA_SxCR_MSIZE | DMA_SxCR_PSIZE | DMA_SxCR_MINC;

    while (current_len > 0)
    {
        // 必须确保DMA已关闭才能修改寄存器
        // 虽然循环末尾等待了传输完成，但双重保险是修改NDTR/M0AR前的标准操作
        DMA_Cmd(DMA1_Stream4, DISABLE); 
        while(DMA1_Stream4->CR & DMA_SxCR_EN); 

        // 设置内存地址
        DMA1_Stream4->M0AR = (uint32_t)current_data;

        uint32_t dma_count;

        // --- 读取当前CR寄存器值 ---
        uint32_t tmp_cr = DMA1_Stream4->CR;
        
        // --- 关键步骤：先清除 PSIZE, MSIZE, MINC 的旧状态 ---
        tmp_cr &= ~CR_MASK_TO_CLEAR;

        if (spixel)
        {
            // === 16位模式配置 (单色填充) ===
            // 每次传2字节，传输次数 = 字节总长 / 2
            uint32_t items = current_len >> 1; 
            dma_count = items < 65535 ? items : 65535;

            // 设置 PSIZE=01(16bit), MSIZE=01(16bit)
            // MINC 保持为0 (已在上面清除了)
            tmp_cr |= DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0;
        }
        else
        {
            // === 8位模式配置 (图片/文字) ===
            dma_count = current_len < 65535 ? current_len : 65535;

            // 设置 MINC=1 (内存自增)
            // PSIZE 和 MSIZE 保持为00 (8bit, 已在上面清除了)
            tmp_cr |= DMA_SxCR_MINC;
        }

        // --- 写回 CR 寄存器 ---
        DMA1_Stream4->CR = tmp_cr;
        
        // 设置传输数量
        DMA1_Stream4->NDTR = dma_count;

        // 开启 DMA
        DMA_Cmd(DMA1_Stream4, ENABLE);
        
        // 等待传输完成
        xSemaphoreTake(write_gram_semaphore, portMAX_DELAY);
        
        // 计算剩余长度和指针偏移
        if (spixel)
        {
            current_len -= dma_count * 2;
        }
        else
        {
            current_len -= dma_count;
            current_data += dma_count; 
        }
    }

    while (SPI_GetFlagStatus(lcd->SPI, SPI_FLAG_BSY) != RESET);
    GPIO_SetBits(lcd->Port, lcd->CSPin);
    
    // 建议：恢复SPI到默认8位状态（可选，防止影响其他SPI设备或寄存器操作）
    SPI_DataSizeConfig(lcd->SPI, SPI_DataSize_8b);
}



void st7789_fill_color(lcd_desc_t lcd, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{

    if (!in_screen_range(x1, y1, x2, y2))
        return;
    
    st7789_set_range_and_prepare_gram(lcd, x1, y1, x2, y2);
    
    uint32_t pixels = (x2 - x1 + 1) * (y2 - y1 + 1);
    st7789_write_gram(lcd, (uint8_t*)&color, pixels*2, true);
}

static void st7789_draw_font(lcd_desc_t lcd, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *model, uint16_t color, uint16_t bg_color)
{
    uint16_t bytes_per_row = (width + 7) / 8;
    
    static uint8_t buff[72*72*2];
    uint8_t *pbuf = buff;
    for(uint16_t row=0; row<height; row++)
    {
        const uint8_t *row_data = model + row * bytes_per_row;
        for(uint16_t col=0; col<width; col++)
        {
            uint8_t pixel = row_data[col/8] & (1<<(7-col%8));
            uint16_t pixel_color = pixel ? color : bg_color;
            
            // 【修改处】改为大端模式存入：先存高8位，再存低8位
            *pbuf++ = (pixel_color >> 8) & 0xff; // High Byte
            *pbuf++ = pixel_color & 0xff;        // Low Byte
        }
    }
    st7789_set_range_and_prepare_gram(lcd, x, y, x + width - 1, y + height - 1);
    
    // 这里的 false 会触发 write_gram 进入 8位模式，正确发送上面的大端数据
    st7789_write_gram(lcd, buff, pbuf-buff, false); 
}

static const uint8_t *ascii_get_model(char ch, const font_t *font)
{
    uint16_t bytes_per_row = (font->size / 2 + 7) / 8;//计算每行所需字节数，向上取整
    uint16_t bytes_per_char = font->size * bytes_per_row;
    if(font->ascii_map)
    {
        const char *map = font->ascii_map;
        do
        {
            if(*map == ch)
            {
    
                return font->ascii_model + (map - font->ascii_map)* bytes_per_char;
            }
           
        } while (*(++map) != '\0');
        
    }
    else
    {
        return font->ascii_model + (ch - ' ') * bytes_per_char;
    }
    return NULL;
}


static void st77889_write_ascii(lcd_desc_t lcd, uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, const font_t *font)
{

    if(font==NULL)
        return;
    if(ch<' ' || ch>'~')
        return;

    uint16_t fheight = font->size, fwidth = font->size / 2;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;
    
    const uint8_t *model = ascii_get_model(ch, font);
    if(model)
        st7789_draw_font(lcd, x, y, fwidth, fheight, model, color, bg_color);

    //以 'A' - ' ' = 65 - 32 = 33为基准，计算字符在字体模型中的偏移量
    // model = font->model + 33 × 16 × 1 = font->model + 528字节偏移
    
   
}

static void st77889_write_chinese(lcd_desc_t lcd, uint16_t x, uint16_t y, const char *ch, uint16_t color ,uint16_t bg_color, const font_t *font)
{
    if(font==NULL||ch==NULL)
        return;
   
    
    const font_chinese_t *c = font->chinese;
    for(;c->name!=NULL;c++)
    {
        if(strcmp(c->name, ch)==0)
            break;
    }
    if(c->name==NULL)
        return;

    uint16_t fheight = font->size, fwidth = font->size;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;


    
    st7789_draw_font(lcd, x, y, fwidth, fheight, c->model, color, bg_color);
}

static bool is_gb2312(char ch)
{
    return ((unsigned char)ch >= 0xa1) && ((unsigned char)ch <= 0xf7);
}

// static int utf8_char_length(const char *str)
// {
//     if((*str & 0x80) == 0)
//         return 1;
//     else if((*str & 0xe0) == 0xc0)
//         return 2;
//     else if((*str & 0xf0) == 0xe0)
//         return 3;
//     else if((*str & 0xf8) == 0xf0)
//         return 4;
//     else
//         return -1;
// }
// 自定义字符映射表
static const char* font_ascii_map[] = {
    NULL,               // font16 - 标准 ASCII (95 字符)
    NULL,               // font20 - 标准 ASCII
    NULL,               // font24 - 标准 ASCII
    NULL,               // font32 - 标准 ASCII
    "0123456789: -",    // font54 - 自定义 (13 字符)
    "0123456789: -",    // font64 - 自定义 (13 字符)
    "0123456789: -",    // font76 - 自定义 (13 字符)
};

static int get_font_map_index(uint8_t font_size)
{
    switch(font_size) {
        case 16: return 0;
        case 20: return 1;
        case 24: return 2;
        case 32: return 3;
        case 54: return 4;
        case 64: return 5;
        case 76: return 6;
        default: return 0;
    }
}

// 在自定义映射表中查找字符索引
static int find_char_in_map(const char *map, char ch)
{
    if(map == NULL) {
        // 标准 ASCII
        if(ch >= ' ' && ch <= '~') {
            return ch - ' ';
        }
        return -1;
    }
    
    // 自定义映射
    const char *p = map;
    int index = 0;
    while(*p) {
        if(*p == ch) {
            return index;
        }
        p++;
        index++;
    }
    return -1;  // 字符不在映射表中
}


static uint32_t get_w25q16_font_addr(uint8_t font_size)
{
    switch(font_size) {
        case 16: return W25Q16_FONT16_ADDR;
        case 20: return W25Q16_FONT20_ADDR;
        case 24: return W25Q16_FONT24_ADDR;
        case 32: return W25Q16_FONT32_ADDR;
        case 54: return W25Q16_FONT54_ADDR;
        case 64: return W25Q16_FONT64_ADDR;
        case 76: return W25Q16_FONT76_ADDR;
        default: return W25Q16_FONT16_ADDR;
    }
}


static void w25q16_get_ascii_model(w25q16_desc_t w25q16, uint8_t font_size, char ch, uint8_t *buf)
{
    // 获取字符映射表
    int map_idx = get_font_map_index(font_size);
    const char *map = font_ascii_map[map_idx];
    
    // 查找字符索引
    int char_index = find_char_in_map(map, ch);
    if(char_index < 0) {
        // 字符不存在，填充空白
        uint16_t fwidth = font_size / 2;
        uint16_t bytes_per_row = (fwidth + 7) / 8;
        uint16_t bytes_per_char = font_size * bytes_per_row;
        memset(buf, 0, bytes_per_char);
        return;
    }
    
    // 计算字模大小（半宽）
    uint16_t fwidth = font_size / 2;
    uint16_t bytes_per_row = (fwidth + 7) / 8;
    uint16_t bytes_per_char = font_size * bytes_per_row;
    
    // 计算地址
    uint32_t addr = get_w25q16_font_addr(font_size);
    addr += char_index * bytes_per_char;
    
    // 读取字模
    w25q16_read_data(w25q16, addr, buf, bytes_per_char);
}

// 从 W25Q16 查找汉字字模
static bool w25q16_find_chinese(w25q16_desc_t w25q16, uint8_t qh, uint8_t ql, uint8_t *buf, uint8_t font_size)
{
    // 【修复】正确计算字节大小
    uint16_t bytes_per_row = (font_size + 7) / 8;
    uint32_t bytes_per_char = (uint32_t)font_size * bytes_per_row;
    uint32_t entry_size = 2 + bytes_per_char;  // 编码(2字节) + 字模
    
    uint32_t addr = get_w25q16_font_addr(font_size);
    
    // 【修复】ASCII 字模大小计算 - 宽度是 font_size/2，不是 (font_size+1)/2
    uint16_t ascii_width = font_size / 2;
    uint16_t ascii_bytes_per_row = (ascii_width + 7) / 8;
    uint32_t ascii_bytes_per_char = (uint32_t)font_size * ascii_bytes_per_row;
    addr += 95 * ascii_bytes_per_char;
    
    // 遍历查找汉字
    for(int i = 0; i < 20; i++) {  // 最多查找3000个汉字
        uint8_t qh_read, ql_read;
        w25q16_read_data(w25q16, addr, &qh_read, 1);
        w25q16_read_data(w25q16, addr + 1, &ql_read, 1);
        
        // 检查是否到达结束标记
        if (qh_read == 0x00 && ql_read == 0x00) {
            return false;  // 未找到
        }
        
        if (qh_read == qh && ql_read == ql) {
            w25q16_read_data(w25q16, addr + 2, buf, bytes_per_char);
            return true;  // 找到！
        }
        
        addr += entry_size;
    }
    
    return false;
}

// 显示字符串（从 W25Q16 读取字体）
void st7789_write_string_w25q16(w25q16_desc_t w25q16, lcd_desc_t lcd,
                                 uint16_t x, uint16_t y,
                                 const char *str,
                                 uint16_t color, uint16_t bg_color,
                                 uint8_t font_size)
{
    uint16_t x0 = x;
    uint8_t model[768];
    
    while(*str) {
        uint8_t ch = (uint8_t)*str;
        
        if(ch >= 0xA1 && ch <= 0xF7) {
            // 汉字 (GB2312)
            uint8_t qh = ch;
            uint8_t ql = (uint8_t)*(str + 1);
            
            if(ql >= 0xA1 && ql <= 0xFE) {
                // 【修复】先检查是否需要换行（汉字宽度 = font_size）
                if(x + font_size > 240) {
                    x = x0;
                    y += font_size;
                }
                
                if(w25q16_find_chinese(w25q16, qh, ql, model, font_size)) {
                    st7789_draw_font(lcd, x, y, font_size, font_size, model, color, bg_color);
                } else {
                    st7789_fill_color(lcd, x, y, x + font_size - 1, y + font_size - 1, 0xF800);
                }
                x += font_size;
                str += 2;
            } else {
                str++;
            }
        } else if(ch >= ' ' && ch <= '~') {
            // ASCII
            uint16_t char_width = font_size / 2;
            
            // 【修复】先检查是否需要换行（ASCII 宽度 = font_size / 2）
            if(x + char_width > 240) {
                x = x0;
                y += font_size;
            }
            
            w25q16_get_ascii_model(w25q16, font_size, ch, model);
            st7789_draw_font(lcd, x, y, char_width, font_size, model, color, bg_color);
            x += char_width;
            str++;
        } else if(ch == '\n') {
            x = x0;
            y += font_size;
            str++;
        } else {
            str++;
        }
        
        // 【删除】移除这里的自动换行，已在上面处理
    }
}


void w25q16_show_img(w25q16_desc_t w25q16, lcd_desc_t lcd, uint16_t x, uint16_t y, img_id_t img_id)
{
    // 从图片起始地址开始遍历查找
    uint32_t addr = W25Q16_IMG_START_ADDR;
    
    printf("Looking for img[%u] from 0x%06lX\r\n", img_id, addr);
    
    while(1) {
        // 【优化】一次性读取头部 5 字节: ID(1) + Width(2) + Height(2)
        uint8_t header[5];
        w25q16_read_data(w25q16, addr, header, 5);
        
        uint8_t id_read = header[0];
        
        // 检查结束标记
        if(id_read == 0xFF) {
            printf("Img[%u] not found!\r\n", img_id);
            return;
        }
        
        // 解析宽高（小端格式）
        uint16_t width = header[1] | ((uint16_t)header[2] << 8);
        uint16_t height = header[3] | ((uint16_t)header[4] << 8);
        uint32_t img_size = (uint32_t)width * height * 2;
        
        // 找到目标图片
        if(id_read == (uint8_t)img_id) {
            
            printf("Found Img[%u] at 0x%06lX\r\n", img_id, addr);
            
            // 检查屏幕边界
            if(x + width > LCD_WIDTH || y + height > LCD_HEIGHT) {
                printf("Img out of screen! x=%u y=%u w=%u h=%u\r\n", x, y, width, height);
                return;
            }
            
            // 设置显示区域
            st7789_set_range_and_prepare_gram(lcd, x, y, x + width - 1, y + height - 1);
            
            // 图片数据起始地址: 跳过 ID(1) + Width(2) + Height(2) = 5字节
            uint32_t data_addr = addr + 5;
            
            // 分块读取并显示（避免一次性分配大内存）
            uint32_t remaining = img_size;
            uint32_t chunk_size = 2048;  // 每次读取2KB
            uint8_t *buf = pvPortMalloc(chunk_size);
            
            if(buf == NULL) {
                printf("Malloc failed!\r\n");
                return;
            }
            
            while(remaining > 0) {
                uint32_t chunk = (remaining > chunk_size) ? chunk_size : remaining;
                w25q16_read_data(w25q16, data_addr, buf, chunk);
                st7789_write_gram(lcd, buf, chunk, false);
                data_addr += chunk;
                remaining -= chunk;
            }
            
            vPortFree(buf);
            printf("Img display done!\r\n");
            return;
        }
        
        // 跳到下一张图片: ID(1) + Width(2) + Height(2) + Data(N) = 5 + img_size
        addr += 5 + img_size;
    }
}


void st7789_write_string(lcd_desc_t lcd, uint16_t x, uint16_t y,const char *str, uint16_t color, uint16_t bg_color,const font_t *font)
{
    while(*str)
    {
        //int len = utf8_char_length(*str);
        int len = is_gb2312(*str) ? 2 : 1;
        if(len<=0)
        {
            str++;
            continue;
        }
        else if(len==1)
        {
            st77889_write_ascii(lcd, x, y, *str, color, bg_color, font);
            str++;
            x+=font->size/2;
        }
        else
        {
            char ch[5]={0};// 初始化为全0，确保有字符串结束符
            strncpy(ch,str,len);
            st77889_write_chinese(lcd, x, y, ch, color, bg_color, font);
            str+=len;
            x+=font->size;
        }
    
    }
}

void st7789_draw_image(lcd_desc_t lcd, uint16_t x, uint16_t y, const img_t *img)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || 
        x+img->width-1 >= LCD_WIDTH || y+img->height-1 >= LCD_HEIGHT)
        return ;
   
    st7789_set_range_and_prepare_gram(lcd, x, y, x + img->width - 1, y + img->height - 1);
    
    st7789_write_gram(lcd, (uint8_t *)img->data, img->height*img->width*2, false);

}

void DMA1_Stream4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET)
    {
        BaseType_t pxHigherPriorityTaskWoken;
        xSemaphoreGiveFromISR(write_gram_semaphore, &pxHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
    }
}
