#ifndef __DELAY_H__
#define __DELAY_H__
#include <stdint.h>

typedef void (*cpu_periodic_callback_t)(void);


void cpu_tick_init(void);
void cpu_delay_ms(uint32_t ms);
void cpu_delay_us(uint32_t us);
uint64_t cpu_now(void);
uint64_t cpu_get_us(void);
uint64_t cpu_get_ms(void);
void cpu_register_periodic_callback(cpu_periodic_callback_t callback);


#endif // !__DELAY_H__
