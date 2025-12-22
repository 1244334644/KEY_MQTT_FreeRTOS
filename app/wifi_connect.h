#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>



void wireless_init(void);
void wireless_wait_connect(void);

#endif // WIFI_CONNECT_H