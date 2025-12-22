#ifndef __BOARD_H__
#define __BOARD_H__


#include "led.h"
#include "key.h"

void board_init(void);
extern led_desc_t led1;
extern led_desc_t led2;
extern key_desc_t key1;
extern key_desc_t key2;

#endif//__BOARD_H__