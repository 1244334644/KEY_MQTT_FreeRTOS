#ifndef _ONENET_H_
#define _ONENET_H_

#include <stdint.h>
#include <stdbool.h>

// 【新增】状态改变标志，用于异步上报
extern bool state_changed;

_Bool OneNET_RegisterDevice(void);

_Bool OneNet_DevLink(void);

void OneNet_SendData(void);

void OneNET_Subscribe(void);

void OneNet_RevPro(unsigned char *cmd);

#endif
