#ifndef __WEATHER_H__
#define __WEATHER_H__


#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	char city[32];
	char loaction[128];
	char weather[16];
	int weather_code;
	float temperature;
} weather_info_t;

bool parse_seniverse_response(const char *response, weather_info_t *info);





#endif /* __WEATHER_H__ */