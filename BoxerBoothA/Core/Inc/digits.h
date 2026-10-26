/* สร้างอัตโนมัติโดย tools/screens/gen_digits.py - ห้ามแก้มือ */
#ifndef DIGITS_H
#define DIGITS_H

#include <stdint.h>

/* ตัวอักษรที่มี: 0-9 แล้วตามด้วย . และ /  (เว้นวรรค = วาดพื้นอย่างเดียว) */
typedef struct {
    uint8_t w;
    uint8_t h;
    const uint8_t *alpha;   /* 12 ตัว เรียงต่อกัน ตัวละ w*h ไบต์ */
} digit_font_t;

extern const digit_font_t digits16;   /* เซลล์ 10x16 */
extern const digit_font_t digits24;   /* เซลล์ 14x24 */
extern const digit_font_t digits32;   /* เซลล์ 19x32 */
extern const digit_font_t digits40;   /* เซลล์ 24x40 */
extern const digit_font_t digits48;   /* เซลล์ 29x48 */

#endif
