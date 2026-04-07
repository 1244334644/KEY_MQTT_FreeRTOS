#ifndef __W25Q16_BURN_H__
#define __W25Q16_BURN_H__

#include <stdint.h>
#include "w25q16_desc.h"
#include "font.h"
#include "img.h"
#include "w25q16.h"



// 存储管理结构
typedef struct {
    uint32_t base_addr;      // 起始地址
    uint32_t current_addr;   // 当前写入地址
    uint32_t end_addr;       // 结束地址（边界保护）
} w25q16_storage_t;


// ============== 存储地址定义 ==============
#define W25Q16_TOTAL_SIZE       0x200000    // 2MB

// 索引表
#define W25Q16_INDEX_ADDR       0x000000
#define W25Q16_INDEX_SIZE       0x001000    // 4KB

// 字体区
#define W25Q16_FONT16_ADDR      0x001000
#define W25Q16_FONT16_SIZE      0x005000    // 16KB

#define W25Q16_FONT20_ADDR      0x005000
#define W25Q16_FONT20_SIZE      0x00C000    // 32KB

#define W25Q16_FONT24_ADDR      0x00C000
#define W25Q16_FONT24_SIZE      0x014000    // 32KB

#define W25Q16_FONT32_ADDR      0x014000
#define W25Q16_FONT32_SIZE      0x01E000    // 40KB

#define W25Q16_FONT54_ADDR      0x01E000
#define W25Q16_FONT54_SIZE      0x023000    // 20KB

#define W25Q16_FONT64_ADDR      0x023000
#define W25Q16_FONT64_SIZE      0x028000    // 20KB

#define W25Q16_FONT76_ADDR      0x028000
#define W25Q16_FONT76_SIZE      0x02F800    // 30KB


// 图片区
#define W25Q16_IMG_START_ADDR   0x048000





// 用户数据区
#define W25Q16_USER_ADDR        0x1E0000
#define W25Q16_USER_SIZE        0x020000    // 128KB


void w25q16_burn_font(w25q16_desc_t w25q16);
void w25q16_burn_img(w25q16_desc_t w25q16);



#endif // !__W25Q16_BURN_H__
