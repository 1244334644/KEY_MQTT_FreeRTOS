#ifndef __AHT20_H__
#define __AHT20_H__

#include <stdbool.h>
#include <stdint.h>

struct aht20_desc;
typedef struct aht20_desc *aht20_desc_t;

bool aht20_Init(aht20_desc_t aht20);
bool aht20_start_measure(aht20_desc_t aht20);
bool aht20_wait_for_measure(aht20_desc_t aht20);
bool aht20_read_measurement(aht20_desc_t aht20, float *temp, float *humidity);


#endif /* __AHT20_H__ */
