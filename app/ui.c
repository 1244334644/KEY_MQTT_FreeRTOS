#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lcd.h"
#include "lcd_desc.h"
#include "font.h"
#include "img.h"
#include "ui.h"
#include "board.h"

typedef enum
{
    UI_ACTION_FILL_COLOR,
    UI_ACTION_WRITE_STRING,
    UI_ACTION_DRAW_IMAGE,
} ui_action_t;


typedef struct
{
    ui_action_t action;
    union
    {
        struct
        {
            uint16_t x;
            uint16_t y;
            uint16_t width;
            uint16_t height;
            uint16_t color;
        } fill_color;
        struct
        {
            uint16_t x;
            uint16_t y;
            const char *str;
            uint16_t color;
            uint16_t bg_color;
            const font_t *font;
        } write_string;
        struct
        {
            uint16_t x;
            uint16_t y;
            const img_t *img;
        } draw_image;
    };
}ui_message_t;

static QueueHandle_t ui_queue;


static void ui_func(void *param)
{
    ui_message_t msg;

    lcd_init(lcd);

   while (1)
   {
        xQueueReceive(ui_queue, &msg, portMAX_DELAY);
        switch (msg.action)
        {
        case UI_ACTION_FILL_COLOR:
            st7789_fill_color(lcd, msg.fill_color.x, msg.fill_color.y, 
                msg.fill_color.width, msg.fill_color.height, msg.fill_color.color);
            break;
        case UI_ACTION_WRITE_STRING:
            st7789_write_string(lcd, msg.write_string.x, msg.write_string.y, msg.write_string.str, 
                msg.write_string.color, msg.write_string.bg_color, msg.write_string.font);
            vPortFree((void*)msg.write_string.str);
            break;
        case UI_ACTION_DRAW_IMAGE:
            st7789_draw_image(lcd, msg.draw_image.x, msg.draw_image.y, msg.draw_image.img);
            break;
        default:
            printf("ui_func: unknown action %d\n", msg.action);
            break;
        }
   }
   
}

void ui_init(lcd_desc_t lcd)
{
    ui_queue = xQueueCreate(16, sizeof(ui_message_t));
    configASSERT(ui_queue);

    xTaskCreate(ui_func, "ui", 1024, NULL, 8, NULL);
}
void ui_fill_color(lcd_desc_t lcd, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    ui_message_t ui_msg;
    ui_msg.action = UI_ACTION_FILL_COLOR;
    ui_msg.fill_color.x = x1;
    ui_msg.fill_color.y = y1;
    ui_msg.fill_color.width = x2;
    ui_msg.fill_color.height = y2;
    ui_msg.fill_color.color = color;
    xQueueSend(ui_queue, &ui_msg, 0);

}
void ui_write_string(lcd_desc_t lcd, uint16_t x, uint16_t y,const char *str, uint16_t color, uint16_t bg_color,const font_t *font)
{
    char *pstr = pvPortMalloc(strlen(str)+1);
    if(pstr == NULL)
    {
        printf("ui_write_string: malloc failed\n");
        return;
    }
    strcpy(pstr, str);


    ui_message_t ui_msg;
    ui_msg.action = UI_ACTION_WRITE_STRING;
    ui_msg.write_string.x = x;
    ui_msg.write_string.y = y;
    ui_msg.write_string.str = pstr;
    ui_msg.write_string.color = color;
    ui_msg.write_string.bg_color = bg_color;
    ui_msg.write_string.font = font;

    xQueueSend(ui_queue, &ui_msg, 0);

}
void ui_draw_image(lcd_desc_t lcd, uint16_t x, uint16_t y, const img_t *img)
{
    ui_message_t ui_msg;
    ui_msg.action = UI_ACTION_DRAW_IMAGE;
    ui_msg.draw_image.x = x;
    ui_msg.draw_image.y = y;
    ui_msg.draw_image.img = img;
    xQueueSend(ui_queue, &ui_msg, 0);

}

