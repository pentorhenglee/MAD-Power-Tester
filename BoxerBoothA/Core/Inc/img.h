/* img.h - วาดภาพที่เก็บแบบ RLE และตัวเลขฟอนต์ที่อบไว้ (ไฟล์นี้เหมือนกันทั้งสองบอร์ด) */
#ifndef IMG_H
#define IMG_H

#include <stdint.h>
#include "digits.h"

/* วาดภาพเต็มจอ 240x320 จากข้อมูล RLE ที่สร้างด้วย tools/screens/png2rle.py */
void IMG_Draw(const uint8_t *rle);

/* วาดตัวเลขด้วยฟอนต์ที่อบไว้ รับได้เฉพาะ 0-9 . / และเว้นวรรค
   fg = สีตัวเลข, bg = สีพื้นตรงนั้น (ต้องเป็นสีเรียบสีเดียว)
   เว้นวรรค = ระบายสีพื้นทั้งช่อง ใช้ลบเลขเก่า */
void IMG_DrawNumber(const digit_font_t *f, int x, int y, const char *s,
                    uint16_t fg, uint16_t bg);

/* สี่เหลี่ยมทึบ ตัดส่วนที่เลยขอบจอทิ้งให้เอง */
void IMG_FillRect(int x, int y, int w, int h, uint16_t c);

/* วงกลมทึบ วาดเป็นเส้นแนวนอนทีละแถว เร็วกว่า ILI9341_Draw_Filled_Circle
   ของไลบรารีราว 30 เท่า เพราะตัวนั้นส่งทีละจุด */
void IMG_FillCircle(int cx, int cy, int r, uint16_t c);

#endif
