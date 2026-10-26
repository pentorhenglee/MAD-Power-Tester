/*
 * leds.c - ไฟเส้น WS2812B 60 ดวง (บอร์ด B) ดูหัวข้อใน leds.h
 *
 * ขับด้วย TIM3_CH1 PWM + DMA1 Stream4 ที่ CubeMX ตั้งไว้แล้ว (PB4 ผ่าน R 470)
 *   แต่ละบิตของสี = หนึ่งคาบ PWM 1.25 us (TIM3 นับ 100 MHz, ARR 124)
 *   บิต 0 = สูง 0.4 us (นับ 40) · บิต 1 = สูง 0.8 us (นับ 80)
 *   DMA เป็นคนเปลี่ยน duty ทีละบิต CPU ไม่ต้องรอ และไม่ต้องปิด interrupt
 *   (ถ้าให้ CPU เปิดปิดขาเอง ต้องปิด interrupt ราว 1.8 ms ทุกครั้ง คำสั่ง UART จากบอร์ด A จะหาย)
 *
 * บัฟเฟอร์ = [ศูนย์ 8 ช่อง][60 ดวง x 24 บิต][ศูนย์ 72 ช่อง]
 *   ศูนย์ท้าย 72 x 1.25 = 90 us = สัญญาณ reset ให้ไฟจำค่า
 *   DMA ตั้งเป็น circular ใน CubeMX เลยต้องสั่งหยุดตอนส่งครบ ถ้าหยุดช้าไปนิดแล้ววนกลับต้นบัฟเฟอร์
 *   ก็เจอศูนย์หัว 8 ช่อง ขายังต่ำ ไฟไม่เพี้ยน
 *
 * โหมด (บอร์ด A สั่งด้วย L<n> ด่านของบอร์ด B สั่งเอง)
 *   LED_OFF       ดับ
 *   LED_ATTRACT   บรรยากาศ: กะพริบตื้ด ๆ เป็นจังหวะ ATTRACT_BPM สีรุ้งเปลี่ยนทุกจังหวะ
 *   LED_PLAY      หลอดวัดพลัง: ยาวตาม Leds_SetLevel 0-100 ไล่สีเขียว -> เหลือง -> แดง
 *                 ขึ้นเร็ว ลงช้า เหมือนเข็มเครื่องวัดแรงต่อย
 *   LED_CELEBRATE รุ้งวิ่ง
 *   LED_WAIT      แดงจาง หายใจช้า ๆ (ด่านกรี๊ด: เงียบก่อน)
 */
#include "leds.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;

#define N_LEDS        60
#define LEAD           8
#define TRAIL         72
#define BUF_LEN       (LEAD + N_LEDS * 24 + TRAIL)   /* 1520 ช่อง ลงตัวกับบรรทัดแคช 32 ไบต์ */
#define DUTY_0        40
#define DUTY_1        80

#define BRIGHT        70      /* เพดานความสว่าง 0-255 (~27%) คูณทุกสีก่อนส่ง กันกินไฟเกิน · ห้ามเกิน 128 */
#define FRAME_MS      25      /* ~40 ภาพ/วิ */
#define ATTRACT_BPM  120      /* จังหวะไฟบรรยากาศ ปรับให้ใกล้เพลง 0001 ได้ ไม่ต้องเป๊ะ */
#define RISE_PER_FRAME 40     /* หลอดวัดพลังขึ้นกี่ส่วนพันต่อภาพ  (~0.6 วิ จากศูนย์ถึงเต็ม) ใช้กับค่าสด */
#define SWEEP_PER_FRAME 18    /* Leds_SweepTo ไต่ช้ากว่า (~1.4 วิ ถึงเต็ม) ให้คนลุ้นว่าจะถึงแดงไหม */
#define FALL_PER_FRAME 12     /* ลงช้ากว่า เห็นยอดค้างนิดนึง */
/* สีของหลอดวัดพลังตามตำแหน่ง 0 = ล่างสุด .. 1000 = บนสุด ไล่สีต่อเนื่องระหว่างจุด
   แบ่งเป็นช่วงเท่า ๆ กัน เขียว -> เหลือง -> ส้ม -> แดง ช่วงละ 1/3 ของแถบ
   ได้ครึ่งหลอดก็เริ่มเห็นส้ม ไม่ใช่เขียวทั้งแถบ
   เขียวลดเหลือ 170 เพราะตาเห็นสีเขียวของ LED สว่างกว่าแดงมาก ถ้าเต็ม 255 ช่วงเขียวจะดูกว้างเกิน */
static const struct { int pos; uint8_t r, g, b; } METER_STOPS[] = {
    {    0,   0, 170, 0 },     /* เขียว */
    {  333, 255, 200, 0 },     /* เหลือง */
    {  667, 255,  80, 0 },     /* ส้ม */
    { 1000, 255,   0, 0 },     /* แดง */
};
#define N_STOPS  (int)(sizeof METER_STOPS / sizeof METER_STOPS[0])

static uint16_t pwm[BUF_LEN] __attribute__((aligned(32)));
static volatile int dma_busy;
static uint8_t  px[N_LEDS][3];          /* R G B ก่อนคูณความสว่าง */

static int      mode = -1;
static int      target;                 /* ระดับที่สั่ง 0-1000 */
static int      shown;                  /* ระดับที่แสดงอยู่ เลื่อนเข้าหา target ทีละนิด */
static int      rise = RISE_PER_FRAME;  /* ความเร็วขาขึ้น: ค่าสดเร็ว · SweepTo ช้า */
static int      sweep;                  /* 1 = โหมดเครื่องวัดแรงต่อย ถึงยอดแล้วดวงบนกะพริบ */
static uint32_t t_frame;
static uint32_t t_mode;                 /* เวลาที่เข้าโหมดปัจจุบัน */

/* ---------- ส่งออกไปที่แถบ ---------- */

static void show(void)
{
    if (dma_busy) return;               /* ภาพก่อนยังส่งไม่จบ ข้ามไปรอบหน้า */

    uint16_t *p = &pwm[LEAD];
    for (int i = 0; i < N_LEDS; i++) {
        uint8_t c[3] = {                /* WS2812B รับลำดับ G R B บิตสูงก่อน */
            (uint8_t)(px[i][1] * BRIGHT / 255),
            (uint8_t)(px[i][0] * BRIGHT / 255),
            (uint8_t)(px[i][2] * BRIGHT / 255),
        };
        for (int k = 0; k < 3; k++)
            for (int bit = 7; bit >= 0; bit--)
                *p++ = (c[k] & (1u << bit)) ? DUTY_1 : DUTY_0;
    }

    /* D-Cache: ดันค่าที่เพิ่งเขียนลงแรมจริงก่อน DMA มาอ่าน */
    SCB_CleanDCache_by_Addr((uint32_t *)pwm, sizeof pwm);
    dma_busy = 1;
    if (HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (const uint32_t *)pwm, BUF_LEN) != HAL_OK)
        dma_busy = 0;
}

/* DMA ส่งครบบัฟเฟอร์ (เรียกจาก interrupt) หยุดก่อนมันวนรอบใหม่ */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        dma_busy = 0;
    }
}

/* ---------- ตัวช่วยสี ---------- */

static void set(int i, int r, int g, int b)
{
    px[i][0] = (uint8_t)r;
    px[i][1] = (uint8_t)g;
    px[i][2] = (uint8_t)b;
}

static void fill(int r, int g, int b)
{
    for (int i = 0; i < N_LEDS; i++) set(i, r, g, b);
}

/* วงล้อสี 0-255: แดง -> เขียว -> น้ำเงิน -> แดง  คูณด้วย v (0-255) */
static void wheel(int i, int hue, int v)
{
    hue &= 255;
    int r, g, b;
    if (hue < 85)       { r = 255 - hue * 3; g = hue * 3;         b = 0; }
    else if (hue < 170) { hue -= 85;  r = 0; g = 255 - hue * 3;   b = hue * 3; }
    else                { hue -= 170; r = hue * 3; g = 0;         b = 255 - hue * 3; }
    set(i, r * v / 255, g * v / 255, b * v / 255);
}

/* ---------- ลวดลายแต่ละโหมด ---------- */

/* ตื้ด ๆ ตามจังหวะ: ต้นจังหวะสว่างสุดแล้วจางลงแบบโค้ง จังหวะที่ 1 ของทุก 4 จังหวะแรงสุด */
static void draw_attract(uint32_t now)
{
    uint32_t beat_ms = 60000u / ATTRACT_BPM;
    uint32_t n       = now / beat_ms;               /* จังหวะที่เท่าไหร่ */
    int      phase   = (int)((now % beat_ms) * 255 / beat_ms);
    int      k       = 255 - phase;
    k = k * k / 255;                                /* จางเร็วตอนต้น = ฟังดูเป็นตื้ด */
    int peak = (n % 4 == 0) ? 255 : 170;
    int v    = 25 + k * (peak - 25) / 255;          /* ไม่ดับสนิท มีแสงค้างนิด ๆ */
    int base = (int)(n * 37);                       /* สีเปลี่ยนทุกจังหวะ */
    for (int i = 0; i < N_LEDS; i++) wheel(i, base + i * 2, v);
}

/* สีที่ตำแหน่ง pos (0-1000): หาสองจุดใน METER_STOPS ที่ pos อยู่ระหว่างกลาง แล้วผสมตามระยะ */
static void meter_color(int i, int pos)
{
    int k = 0;
    while (k < N_STOPS - 2 && pos > METER_STOPS[k + 1].pos) k++;
    int span = METER_STOPS[k + 1].pos - METER_STOPS[k].pos;
    int t    = (pos - METER_STOPS[k].pos) * 256 / span;            /* 0..256 */
    set(i, METER_STOPS[k].r + (METER_STOPS[k + 1].r - METER_STOPS[k].r) * t / 256,
           METER_STOPS[k].g + (METER_STOPS[k + 1].g - METER_STOPS[k].g) * t / 256,
           METER_STOPS[k].b + (METER_STOPS[k + 1].b - METER_STOPS[k].b) * t / 256);
}

/* หลอดวัดพลังแบบตู้จริง: สีผูกกับตำแหน่ง ไม่ใช่กับคะแนน
   ได้น้อย = ติดแค่ช่วงล่างที่เป็นเขียว · ได้เยอะ = ไต่ผ่านเหลือง ส้ม จนถึงแดงบนสุด · ส่วนที่ไม่ถึงดับสนิท */
static void draw_meter(uint32_t now)
{
    int lit = (shown * N_LEDS + 999) / 1000;        /* ปัดขึ้น ได้คะแนนนิดเดียวก็ติดอย่างน้อย 1 ดวง */
    if (shown == 0) lit = 0;

    for (int i = 0; i < N_LEDS; i++) {
        if (i >= lit) { set(i, 0, 0, 0); continue; }
        meter_color(i, i * 1000 / (N_LEDS - 1));    /* 0 ล่างสุด .. 1000 บนสุด */
    }

    /* ถึงยอดแล้ว ดวงบนสุดกะพริบขาว 4 ครั้ง/วิ เหมือนเข็มค้างที่คะแนน */
    if (sweep && lit > 0 && shown == target && (now / 125) % 2)
        set(lit - 1, 255, 255, 255);
}

static void draw_celebrate(uint32_t now)
{
    int base = (int)(now / 6);                      /* รุ้งวิ่ง */
    for (int i = 0; i < N_LEDS; i++) wheel(i, base + i * 8, 255);
}

static void draw_wait(uint32_t now)
{
    int t = (int)((now - t_mode) % 1600);           /* หายใจรอบละ 1.6 วิ */
    int v = (t < 800) ? t : 1600 - t;               /* 0..800 */
    fill(30 + v * 50 / 800, 0, 0);
}

/* ---------- ทางเข้า ---------- */

void Leds_Init(void)
{
    mode   = -1;
    target = shown = 0;
    Leds_SetMode(LED_OFF);
    fill(0, 0, 0);
    show();
}

void Leds_SetMode(int m)
{
    if (m == mode) return;
    mode   = m;
    t_mode = HAL_GetTick();
}

/* 0-100 ใช้กับโหมด PLAY: ด่านกรี๊ดส่งความดังสดทุกรอบ ด่านต่อยส่งคะแนน/10 ตอนหมัดโดน */
void Leds_SetLevel(int l)
{
    if (l < 0)   l = 0;
    if (l > 100) l = 100;
    target = l * 10;
    rise   = RISE_PER_FRAME;
    sweep  = 0;
}

/* เครื่องวัดแรงต่อย: เริ่มจากศูนย์แล้วไต่ขึ้นช้า ๆ ไปหาคะแนน */
void Leds_SweepTo(int l)
{
    Leds_SetLevel(l);
    shown = 0;
    rise  = SWEEP_PER_FRAME;
    sweep = 1;
}

void Leds_Tick(void)
{
    uint32_t now = HAL_GetTick();
    if (now - t_frame < FRAME_MS) return;
    t_frame = now;

    if (shown < target)      { shown += rise; if (shown > target) shown = target; }
    else if (shown > target) { shown -= FALL_PER_FRAME; if (shown < target) shown = target; }

    switch (mode) {
    case LED_ATTRACT:   draw_attract(now);   break;
    case LED_PLAY:      draw_meter(now);     break;
    case LED_CELEBRATE: draw_celebrate(now); break;
    case LED_WAIT:      draw_wait(now);      break;
    default:            fill(0, 0, 0);       break;
    }
    show();
}
