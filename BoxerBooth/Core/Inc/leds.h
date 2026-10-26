/*
 * leds.h - ไฟเส้น WS2812B 60 ดวง (บอร์ด B)
 */
#ifndef LEDS_H
#define LEDS_H

#define LED_OFF      0
#define LED_ATTRACT  1   /* ไฟวิ่งเบา ๆ ตอนรอผู้เล่น */
#define LED_PLAY     2   /* ตอนกำลังเล่นด่าน */
#define LED_CELEBRATE 3  /* ตอนจบเกม */
#define LED_WAIT      4  /* ด่านกรี๊ด: แดงจาง ๆ นิ่ง = เงียบก่อน ห้ามตะโกน */

void Leds_Init(void);
void Leds_SetMode(int mode);
void Leds_SetLevel(int level);   /* 0-100 ใช้ตอนด่านกรี๊ด ให้ไฟวิ่งตามความดัง */
void Leds_SweepTo(int level);    /* 0-100 แบบเครื่องวัดแรงต่อย: ไต่ขึ้นช้า ๆ ให้ลุ้น ถึงยอดแล้วดวงบนสุดกะพริบ */
void Leds_Tick(void);            /* เรียกทุกลูป */

#endif
