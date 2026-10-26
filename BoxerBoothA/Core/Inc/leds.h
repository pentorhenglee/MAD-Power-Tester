/*
 * leds.h (บอร์ด A) - ไฟเส้น WS2812B อยู่บอร์ด B บอร์ด A แค่ส่งคำสั่งเปลี่ยนโหมดไป
 * โหมดเลขเดียวกับ leds.h ของบอร์ด B
 */
#ifndef LEDS_H
#define LEDS_H

#define LED_OFF        0
#define LED_ATTRACT    1   /* ไฟวิ่งเบา ๆ ตอนรอผู้เล่น */
#define LED_PLAY       2   /* ตอนกำลังเล่นด่าน */
#define LED_CELEBRATE  3   /* ตอนจบเกม */
#define LED_WAIT       4   /* ด่านกรี๊ด: แดงจาง ๆ นิ่ง = เงียบก่อน ห้ามตะโกน */

void Leds_SetMode(int mode);

#endif
