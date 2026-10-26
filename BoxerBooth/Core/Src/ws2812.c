#include "ws2812.h"
#include <string.h>

/* ---------- ข้อมูลสีของทุกดวง ---------- */
static uint8_t buf[WS_LED_COUNT][3];   /* buf[ดวง][0=R, 1=G, 2=B] */
static uint8_t bright = 255;

/* ---------- เวลาแต่ละช่วง (หน่วย: clock ของ CPU) ---------- */
static uint32_t t0h;     /* บิต 0: ขาสูงนาน 0.30 us */
static uint32_t t1h;     /* บิต 1: ขาสูงนาน 0.80 us */
static uint32_t tbit;    /* 1 บิตยาว 1.25 us        */
static uint32_t treset;  /* ช่วงพักให้ไฟจำค่า 300 us */

/* ---------- คำสั่งขา GPIO ---------- */
#define PIN_HIGH()  (WS_PORT->BSRR = WS_PIN)
#define PIN_LOW()   (WS_PORT->BSRR = (uint32_t)WS_PIN << 16)


void ws_init(void)
{
  /* 1) ตั้งขาเป็น output ความเร็วสูงสุด */
  GPIO_InitTypeDef g = {0};
  WS_PORT_CLK_EN();
  g.Pin   = WS_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(WS_PORT, &g);
  PIN_LOW();

  /* 2) เปิดตัวนับ clock ของ CPU (DWT) ไว้จับเวลา */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR    = 0xC5ACCE55;          /* STM32F7 ต้องปลดล็อกก่อน */
  DWT->CYCCNT = 0;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

  /* 3) แปลงเวลาเป็นจำนวน clock (200 MHz -> 200 clock ต่อ 1 us) */
  uint32_t mhz = SystemCoreClock / 1000000;
  t0h    = mhz * 300  / 1000;
  t1h    = mhz * 800  / 1000;
  tbit   = mhz * 1250 / 1000;
  treset = mhz * 300;

  /* 4) เริ่มต้นด้วยไฟดับทุกดวง */
  ws_clear();
  ws_show();
}


void ws_set(uint16_t i, uint8_t r, uint8_t g, uint8_t b)
{
  if (i >= WS_LED_COUNT) return;     /* กันเขียนเกินอาร์เรย์ */
  buf[i][0] = r;
  buf[i][1] = g;
  buf[i][2] = b;
}

void ws_fill(uint8_t r, uint8_t g, uint8_t b)
{
  for (uint16_t i = 0; i < WS_LED_COUNT; i++)
    ws_set(i, r, g, b);
}

void ws_clear(void)
{
  memset(buf, 0, sizeof(buf));
}

void ws_brightness(uint8_t level)
{
  bright = level;
}


/* ส่ง 1 ไบต์ (8 บิต) เริ่มจากบิตบนสุด
 *
 *   บิต 0:  ‾‾|________     (สูงสั้น)
 *   บิต 1:  ‾‾‾‾|_____      (สูงยาว)
 *           |<- 1.25 us ->|
 */
static void send_byte(uint8_t v)
{
  for (int bit = 7; bit >= 0; bit--)
  {
    uint32_t high  = (v & (1 << bit)) ? t1h : t0h;
    uint32_t start = DWT->CYCCNT;

    PIN_HIGH();
    while (DWT->CYCCNT - start < high);   /* รอช่วงขาสูง */
    PIN_LOW();
    while (DWT->CYCCNT - start < tbit);   /* รอจนครบ 1 บิต */
  }
}

/* ปรับความสว่าง: 0-255 */
static uint8_t dim(uint8_t c)
{
  return (uint8_t)((c * bright) / 255);
}


void ws_show(void)
{
  /* ปิด interrupt ระหว่างส่ง ไม่งั้นจังหวะเวลาเพี้ยน */
  __disable_irq();

  for (uint16_t i = 0; i < WS_LED_COUNT; i++)
  {
    /* WS2812B รับลำดับ G -> R -> B */
    send_byte(dim(buf[i][1]));
    send_byte(dim(buf[i][0]));
    send_byte(dim(buf[i][2]));
  }

  __enable_irq();

  /* ขาต่ำค้างไว้ > 280 us = บอกไฟว่า "จบเฟรม ให้แสดงผลได้" */
  uint32_t start = DWT->CYCCNT;
  while (DWT->CYCCNT - start < treset);
}
