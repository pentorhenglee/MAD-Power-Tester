/* img.c - ดูหัวข้อใน img.h
 *
 * ภาพถูกเก็บแบบ RLE คือบอกว่า "สีนี้ ซ้ำกันกี่จุด" แทนที่จะเก็บทีละจุด
 * หนึ่งช่วงใช้ 4 ไบต์  [สี hi][สี lo][จำนวน hi][จำนวน lo]  จบไฟล์เมื่อจำนวน = 0
 * ภาพเต็มจอถ้าเก็บดิบใช้ 150 KB แบบนี้เหลือราว 25 KB
 *
 * ไม่ได้ใช้ ILI9341_Draw_Image ของไลบรารีเพราะมันส่งข้อมูล 153,500 ไบต์
 * แต่จอต้องการ 153,600 ขาดไป 100 ไบต์ แถวล่างสุดจะเพี้ยน
 */
#include "img.h"
#include "ILI9341_STM32_Driver.h"
#include "spi.h"

#define SCR_W  240
#define SCR_H  320
#define CHUNK  128                /* ส่งทีละ 128 จุด = 256 ไบต์ */

void IMG_Draw(const uint8_t *rle)
{
    static uint8_t buf[CHUNK * 2];

    /* หน้าต่างต้องเป็น 0..239 กับ 0..319 พอดี (ขอบขวาและขอบล่างนับรวม)
       ถ้าใส่ 240 / 320 แบบที่ไลบรารีใช้ จอจะได้หน้าต่างกว้าง 241 จุด
       แต่ละแถวจะเลื่อนไปหนึ่งจุด ภาพทั้งภาพเบี้ยวเป็นเส้นทแยง */
    ILI9341_Set_Address(0, 0, SCR_W - 1, SCR_H - 1);
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

    while (1) {
        uint8_t  hi = *rle++;
        uint8_t  lo = *rle++;
        uint32_t n  = ((uint32_t)*rle++) << 8;
        n |= *rle++;
        if (n == 0) break;                      /* ช่วงปิดท้าย */

        for (int i = 0; i < CHUNK; i++) {       /* เติมสีเดียวกันทั้งบัฟเฟอร์ */
            buf[i * 2]     = hi;
            buf[i * 2 + 1] = lo;
        }
        while (n > 0) {                         /* ส่งซ้ำจนครบจำนวนจุดของช่วงนี้ */
            uint32_t k = (n > CHUNK) ? CHUNK : n;
            HAL_SPI_Transmit(HSPI_INSTANCE, buf, k * 2, 100);
            n -= k;
        }
    }

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

void IMG_FillRect(int x, int y, int w, int h, uint16_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;
    ILI9341_Draw_Rectangle(x, y, w, h, c);
}

void IMG_FillCircle(int cx, int cy, int r, uint16_t c)
{
    for (int dy = -r; dy <= r; dy++) {
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r) dx--;   /* ความกว้างครึ่งแถวนี้ */
        IMG_FillRect(cx - dx, cy + dy, dx * 2 + 1, 1, c);
    }
}

/* ผสมสองสีตามความทึบ a (0 = bg ล้วน, 255 = fg ล้วน)
   RGB565 เก็บ แดง 5 บิต เขียว 6 บิต น้ำเงิน 5 บิต จึงต้องแยกผสมทีละสี */
static uint16_t blend(uint16_t fg, uint16_t bg, uint8_t a)
{
    int fr = (fg >> 11) & 0x1F, fgn = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    int br = (bg >> 11) & 0x1F, bgn = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    int r = br + (fr - br) * a / 255;
    int g = bgn + (fgn - bgn) * a / 255;
    int b = bb + (fb - bb) * a / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void IMG_DrawNumber(const digit_font_t *f, int x, int y, const char *s,
                    uint16_t fg, uint16_t bg)
{
    static uint8_t buf[48 * 2];          /* หนึ่งแถวของตัวอักษร กว้างสุด 48 จุด */

    for (; *s; s++, x += f->w) {
        int idx;
        if (*s >= '0' && *s <= '9') idx = *s - '0';
        else if (*s == '.')         idx = 10;
        else if (*s == '/')         idx = 11;
        else                        idx = -1;      /* เว้นวรรค = พื้นล้วน */

        if (x < 0 || x + f->w > SCR_W) continue;   /* ไม่วาดตัวที่เลยขอบจอ */
        const uint8_t *a = (idx >= 0) ? f->alpha + idx * f->w * f->h : 0;

        ILI9341_Set_Address(x, y, x + f->w - 1, y + f->h - 1);
        HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

        for (int row = 0; row < f->h; row++) {
            for (int col = 0; col < f->w; col++) {
                uint16_t c = a ? blend(fg, bg, a[row * f->w + col]) : bg;
                buf[col * 2]     = c >> 8;
                buf[col * 2 + 1] = c & 0xFF;
            }
            HAL_SPI_Transmit(HSPI_INSTANCE, buf, f->w * 2, 100);
        }

        HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    }
}
