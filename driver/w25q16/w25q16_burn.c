#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "font.h"
#include "img.h"
#include "usart.h"
#include "w25q16.h"
#include "w25q16_desc.h"
#include "w25q16_burn.h"

static uint32_t font_addr[]={W25Q16_FONT16_ADDR,W25Q16_FONT20_ADDR,W25Q16_FONT24_ADDR,W25Q16_FONT32_ADDR,W25Q16_FONT54_ADDR,W25Q16_FONT64_ADDR,W25Q16_FONT76_ADDR};
static const font_t  *font[]={&font16,&font20,&font24,&font32,&font54,&font64,&font76};

static const img_t *img[]={&img_error,&img_duoyun,&img_leizhenyu,&img_na,&img_qing,&img_wenduji,&img_icon_wifi,
                           &img_yintian,&img_yueliang,&img_zhongxue,&img_zhongyu,&img_wifi,
                           &img_nowifi,&img_welcome,&img_wifi_fill};



// 字体大小数组
static uint8_t font_sizes[] = {16, 20, 24, 32, 54, 64, 76};
static w25q16_storage_t g_storage = {0};
// 初始化存储区域
void w25q16_storage_init(uint32_t start_addr, uint32_t max_size)
{
    g_storage.base_addr = start_addr;
    g_storage.current_addr = start_addr;
    g_storage.end_addr = start_addr + max_size;
}

// 获取当前地址
uint32_t w25q16_storage_get_addr(void)
{
    return g_storage.current_addr;
}

// 烧录字体到 W25Q16
void w25q16_burn_font(w25q16_desc_t w25q16)
{
    printf("=== Burning Font ===\r\n");
    
    // 擦除
    printf("Erasing...\r\n");
    for(uint32_t addr = 0x000000; addr < 0x040000; addr += 0x1000) {
        w25q16_erase_block(w25q16, addr);
        printf(".");
    }
    printf("Done\r\n");
    for(uint8_t i = 0; i < 7; i++)
    {
        uint32_t current_addr = font_addr[i];
        uint8_t font_size = font_sizes[i];  // 【修复】使用实际字体大小
        
        // 【修复】正确计算单个汉字字模大小
        uint16_t bytes_per_row = (font_size + 7) / 8;  // 每行字节数，向上取整
        uint32_t bytes_per_char = (uint32_t)font_size * bytes_per_row;  // 单个汉字总字节数
        
        printf("Font size: %u, bytes_per_char: %lu\r\n", font_size, bytes_per_char);
        
        // 1. 烧录 ASCII 字模
        printf("Writing ASCII (%u bytes)...\r\n", font[i]->font_ascii_size);
        w25q16_write_data(w25q16, current_addr, (uint8_t*)font[i]->ascii_model, font[i]->font_ascii_size);
        current_addr += font[i]->font_ascii_size;
        printf("ASCII end addr: 0x%06lX\r\n", current_addr);
        
        // 2. 烧录汉字
        const font_chinese_t *ch = font[i]->chinese;
        uint32_t chinese_count = 0;
        
        printf("Writing Chinese...\r\n");
        while(ch != NULL && ch->name != NULL) {
            // 获取 GB2312 编码
            uint8_t qh = (uint8_t)ch->name[0];
            uint8_t ql = (uint8_t)ch->name[1];
            
            // 写入: [编码2字节] + [字模N字节]
            uint8_t header[2] = {qh, ql};
            w25q16_write_data(w25q16, current_addr, header, 2);
            w25q16_write_data(w25q16, current_addr + 2, (uint8_t*)ch->model, bytes_per_char);
            
            printf("  [%lu] \"%s\" (0x%02X%02X) at 0x%06lX\r\n", 
                chinese_count, ch->name, qh, ql, current_addr);
            
            current_addr += 2 + bytes_per_char;  // 【修复】使用正确的字节大小
            chinese_count++;
            ch++;
        }
        
        // 写入结束标记
        uint8_t end_mark[2] = {0x00, 0x00};
        w25q16_write_data(w25q16, current_addr, end_mark, 2);
        printf("Chinese count: %lu\r\n", chinese_count);
        printf("Total: %lu bytes\r\n", current_addr - font_addr[i] + 2);

    }
    
    printf("=== Burning Font Complete ===\r\n");
}

// 烧录图片到 W25Q16
void w25q16_burn_img(w25q16_desc_t w25q16)
{
    printf("=== Burning Img ===\r\n");
    
    uint32_t current_addr = W25Q16_IMG_START_ADDR;
    uint8_t img_count = sizeof(img) / sizeof(img[0]);
    
    // 擦除图片区域
    printf("Erasing img area (0x%06X - 0x1E0000)...\r\n", W25Q16_IMG_START_ADDR);
    for(uint32_t addr = W25Q16_IMG_START_ADDR; addr < 0x1E0000; addr += 0x1000) {
        w25q16_erase_block(w25q16, addr);
    }
    printf("Erase Done\r\n");
    
    for(uint8_t i = 0; i < img_count; i++)
    {
        // 【修复】使用 width * height * 2 计算实际大小，忽略 img_size
        uint32_t actual_size = (uint32_t)img[i]->width * img[i]->height * 2;

        printf("Img[%u] at 0x%06lX, W:%u H:%u size:%u\r\n", 
               i, current_addr, img[i]->width, img[i]->height, actual_size);
        
        // 1. 烧录图片ID (1字节)
        uint8_t img_id_byte = (uint8_t)i;
        w25q16_write_data(w25q16, current_addr, &img_id_byte, 1);
        current_addr += 1;
        
        // 2. 烧录图片宽度 (2字节, 小端)
        uint8_t width_bytes[2] = {img[i]->width & 0xFF, (img[i]->width >> 8) & 0xFF};
        w25q16_write_data(w25q16, current_addr, width_bytes, 2);
        current_addr += 2;
        
        // 3. 烧录图片高度 (2字节, 小端)
        uint8_t height_bytes[2] = {img[i]->height & 0xFF, (img[i]->height >> 8) & 0xFF};
        w25q16_write_data(w25q16, current_addr, height_bytes, 2);
        current_addr += 2;
        
        // 4. 烧录图片数据
        w25q16_write_data(w25q16, current_addr, (uint8_t*)img[i]->data, actual_size);
        current_addr += actual_size;
    }
    
    // 【重要】写入结束标记 (ID = 0xFF)
    uint8_t end_mark = 0xFF;
    w25q16_write_data(w25q16, current_addr, &end_mark, 1);
    
    printf("=== Img Burning Complete ===\r\n");
    printf("Total: 0x%06X - 0x%06lX (%lu bytes)\r\n", 
           W25Q16_IMG_START_ADDR, current_addr, current_addr - W25Q16_IMG_START_ADDR);
}




