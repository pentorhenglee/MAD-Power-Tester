#ifndef WS2812_H
#define WS2812_H

#include "main.h"

/* ========== ตั้งค่าตรงนี้ที่เดียว ========== */
#define WS_PORT            GPIOB
#define WS_PIN             GPIO_PIN_4        /* PB4 = CN7 ขา 19 ตามผังขา */
#define WS_PORT_CLK_EN()   __HAL_RCC_GPIOB_CLK_ENABLE()
#define WS_LED_COUNT       60                /* ตัดแถบไว้ 60 ดวง */
/* ========================================== */

void ws_init(void);                                         /* เรียกครั้งเดียวตอนเริ่ม */
void ws_set(uint16_t i, uint8_t r, uint8_t g, uint8_t b);   /* ตั้งสีดวงที่ i         */
void ws_fill(uint8_t r, uint8_t g, uint8_t b);              /* ตั้งสีทุกดวง           */
void ws_clear(void);                                        /* ดับทุกดวง              */
void ws_brightness(uint8_t level);                          /* 0-255                  */
void ws_show(void);                                         /* ส่งข้อมูลออกไปที่ไฟจริง */

#endif
