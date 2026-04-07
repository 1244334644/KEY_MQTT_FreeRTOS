#ifndef __W25Q16_H__
#define __W25Q16_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx.h"


struct w25q16_desc;
typedef struct w25q16_desc *w25q16_desc_t;

void w25q16_init(w25q16_desc_t w25q16); 
void w25q16_SaveByte(w25q16_desc_t w25q16, const uint8_t *data, uint16_t len);
uint8_t w25q16_LoadByte(w25q16_desc_t w25q16);
void w25q16_wait_busy(w25q16_desc_t w25q16);
void w25q16_write_enable(w25q16_desc_t w25q16);
void w25q16_erase_sector(w25q16_desc_t w25q16, uint32_t addr);
void w25q16_erase_block(w25q16_desc_t w25q16, uint32_t addr);
void w25q16_read_data(w25q16_desc_t w25q16, uint32_t addr, uint8_t *buf, uint32_t len);
void w25q16_write_data(w25q16_desc_t w25q16, uint32_t addr, const uint8_t *buf, uint32_t len);


#endif // !__W25Q16_H__
