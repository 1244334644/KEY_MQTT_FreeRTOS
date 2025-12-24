#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "LCD.h"
#include "LCD_desc.h"

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
