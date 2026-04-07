#ifndef __IMG_H__
#define __IMG_H__   

#include <stdint.h>

typedef enum
{
    IMG_ERROR,
    IMG_DUOYUN,
    IMG_LEIZHENYU,
    IMG_NA,
    IMG_QING,
    IMG_WENDUJI,
    IMG_ICON_WIFI,
    IMG_YINTIAN,
    IMG_YUELIANG,
    IMG_ZHONGXUE,
    IMG_ZHONGYU,
    IMG_WIFI,
    IMG_NOWIFI,
    IMG_WELCOME,
    IMG_WIFI_FILL,
    IMG_COUNT,  // 图片总数
} img_id_t;




typedef struct 
{
    img_id_t img_id;
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
    const uint32_t img_size;
} img_t;



extern const img_t img_welcome;
extern const img_t img_error;
extern const img_t img_wifi;
extern const img_t img_icon_wifi;
extern const img_t img_wifi_fill;
extern const img_t img_nowifi;
extern const img_t img_wenduji;

extern const img_t img_na;
extern const img_t img_zhongxue;
extern const img_t img_zhongyu;
extern const img_t img_duoyun;
extern const img_t img_qing;
extern const img_t img_yintian;
extern const img_t img_leizhenyu;
extern const img_t img_yueliang;

#endif // __IMG_H__
