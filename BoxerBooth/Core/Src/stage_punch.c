/*
 * stage_punch.c - ด่านที่ 3 ต่อย
 *
 * คนทำ: Earth
 *
 * กติกาของด่าน: กดปุ่มเขียวเพื่อบอกว่าพร้อม จอเปลี่ยนเป็นเหลืองทั้งแผ่น
 *               แล้วต่อยกระสอบ ครั้งเดียว
 *
 * คะแนน = "พื้นที่ใต้กราฟ" ของสัญญาณเพียโซช่วง 40 ms หลังหมัดโดน ไม่ใช่ยอดสูงสุด
 *   หมัดแรงทำให้สัญญาณเกิน 3.3V วงจรหนีบตัดไว้ ADC อ่านได้ 4095 ทุกหมัด ยอดจึงแยกไม่ออก
 *   แต่หมัดยิ่งแรง สัญญาณยิ่งค้างที่เพดานนานและสั่นค้างแรง -> ผลรวมยังเพิ่มต่อได้
 *
 * ระวัง: เพียโซต้องผ่านวงจรหนีบแรงดันเท่านั้น ต่อตรงขา PC0 พัง
 */
#include "stage.h"
#include "img.h"
#include "digits.h"
#include "main.h"
#include "sound.h"
#include "adc_dma.h"
#include "leds.h"
#include <math.h>

/* คาลิเบรตบนโครงจริง 26 ต.ค. (พื้นที่ที่วัดได้): เบา ~424k · กลาง ~886k · แรง ~1.03M · แรงสุด ~2.2M
   คะแนนใช้โค้งยกกำลัง ดู punch_score() */
#define MIN_AREA     300000     /* หมัดต้องได้พื้นที่เกินนี้ถึงนับว่าต่อย (ไม่ถึง = แค่แตะ รอต่อยใหม่) · หักออกก่อนคิดคะแนน */
#define MAX_AREA    1900000     /* พื้นที่ที่ได้ 1000 คะแนน - ตั้งต่ำกว่าแรงสุดที่วัดได้ ให้หมัดแรงจริงไปถึง 800+ ได้ */
#define SCORE_CURVE    0.7f     /* 1.0 = เส้นตรง (กระจุกช่วงล่าง) · 0.5 = รากที่สอง (กระจุกช่วงบน) · 0.7 กระจายเท่า ๆ กัน */
#define AREA_BASE       300     /* ส่วนที่ต่ำกว่านี้ไม่นับ ตอนนิ่งวัดได้ ~220 จะได้ไม่บวก noise */
#define CAPTURE_BLOCKS    8     /* เก็บพื้นที่กี่ช่วง ช่วงละ ~5 ms -> ~40 ms */
#define SETTLE_MS       800     /* หลังหมัดโดน รอให้กระสอบนิ่งก่อนอ่านจบ */
#define TIMEOUT_MS    15000     /* กดพร้อมแล้วไม่ต่อยสักที ก็ปล่อยผ่าน */
#define READY_TIMEOUT_MS 30000  /* ไม่กดปุ่มเขียวเลย ก็ปล่อยผ่าน ไม่งั้นบอร์ด B ค้างอยู่ด่านนี้ตลอด */
#define BTN_LOCKOUT_MS  300     /* กดแล้วไม่รับซ้ำช่วงนี้ กันหน้าสัมผัสเด้ง */
#define BLINK_MS        250     /* ไฟปุ่มเขียวกะพริบ ติด/ดับ ช่วงละเท่านี้ ตอนรอให้กด */

/* ไฟในปุ่มเขียว: ขั้ว + ต่อ PG2 ขั้ว - ต่อ GND (หลอดมี R ในตัว ไม่ต้องใส่เพิ่ม กินไฟ ~3 mA)
   ตั้งขาในโค้ดเอง ไม่ต้องแก้ .ioc · PG2 ว่างบนบอร์ด B และไม่ได้เปิด EXTI จึงไม่ชนกับใคร */
#define LAMP_PORT       GPIOG
#define LAMP_PIN        GPIO_PIN_2

typedef enum { P_WAIT_READY = 0, P_ARMED, P_SETTLING } punch_phase_t;

static punch_phase_t phase;
static uint32_t t_phase;
static int      peak;           /* ยอดสูงสุด เก็บไว้ดูตอนคาลิเบรตเท่านั้น */
static uint32_t area;           /* ตัวที่ใช้คิดคะแนน */

/* ============================================================
 *  ช่อง 1 - อ่านปุ่มเขียว "พร้อมต่อย"
 *  ปุ่มอยู่ที่ PG3 ตั้ง EXTI3 ขอบขาลง + Pull-up ไว้ใน CubeMX แล้ว กด = อ่านได้ 0
 *  ISR แค่ตั้งธง ลูปหลักมาเช็กอีกทีว่ายังกดอยู่จริง (ห่างกัน ~20 ms = debounce ฟรี)
 * ============================================================ */
static volatile uint8_t btn_edge;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BTN_READY_Pin) btn_edge = 1;
}

static int ready_pressed(void)
{
    static uint32_t t_last;

    if (!btn_edge) return 0;
    btn_edge = 0;

    if (HAL_GPIO_ReadPin(BTN_READY_GPIO_Port, BTN_READY_Pin) != GPIO_PIN_RESET)
        return 0;                           /* ขอบเด้งตอนปล่อย ไม่ได้กดจริง */

    uint32_t now = HAL_GetTick();
    if (now - t_last < BTN_LOCKOUT_MS) return 0;
    t_last = now;
    return 1;
}

static void lamp(int on)
{
    HAL_GPIO_WritePin(LAMP_PORT, LAMP_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void lamp_init(void)
{
    static int done;
    if (done) return;
    done = 1;

    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOG_CLK_ENABLE();
    g.Pin   = LAMP_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LAMP_PORT, &g);
    lamp(0);
}

/* ============================================================
 *  ช่อง 3 - ระดับที่ถือว่า "โดนแล้ว"  (ใช้แค่เริ่มจับ ไม่ได้ใช้คิดคะแนน)
 *  ตั้งไว้ 30% ของเต็มสเกล ตอนไม่แตะวัดได้ ~220 จึงห่าง noise มาก แตะเบาไม่นับ
 *  ถ้าต่อยแล้วไม่ติด ให้ลดลง · ถ้าติดเองโดยไม่มีใครต่อย ให้ขยับขึ้น
 * ============================================================ */
#define HIT_THRESHOLD   (4095 * 30 / 100)   /* 30% ของเต็มสเกล ADC = 1228 */

/* ============================================================
 *  ช่อง 2 - อ่านค่าจากเพียโซ
 *  เพียโซอยู่ที่ PC0 = ADC1_IN10 ช่องที่ 2 ของ 2 ช่อง -> ตัวอย่างคี่ในบัฟเฟอร์
 *  adc_dma.c ล้างแคชให้แล้วค่อยเรียก hook นี้ทุกครึ่งบัฟเฟอร์ (~5 ms)
 *
 *  การจับหมัดทำใน ISR ทั้งหมด จะได้นับช่วงเวลาเป๊ะ ไม่ขึ้นกับลูปหลักที่วนทุก ~20 ms
 *    cap_armed = 1  ลูปหลักเปิดรับหมัด
 *    ช่วงไหนยอดเกินเกณฑ์ -> เริ่มนับ CAPTURE_BLOCKS ช่วง บวกพื้นที่สะสม
 *    ครบแล้ว cap_done = 1 ลูปหลักมาหยิบ cap_area / cap_peak ไป
 * ============================================================ */
static volatile uint8_t  cap_armed;
static volatile uint8_t  cap_blocks;     /* เหลืออีกกี่ช่วง 0 = ไม่ได้จับอยู่ */
static volatile uint8_t  cap_done;
static volatile uint32_t cap_area;
static volatile uint16_t cap_peak;

void Punch_OnAdcBlock(const uint16_t *p, int pairs)
{
    uint16_t m = 0;
    uint32_t a = 0;
    for (int i = 0; i < pairs; i++) {
        uint16_t v = p[2 * i + 1];
        if (v > m) m = v;
        if (v > AREA_BASE) a += v - AREA_BASE;
    }

    if (cap_armed && !cap_blocks && m > HIT_THRESHOLD) {
        cap_armed  = 0;
        cap_area   = 0;
        cap_peak   = 0;
        cap_blocks = CAPTURE_BLOCKS;
    }
    if (cap_blocks) {
        cap_area += a;
        if (m > cap_peak) cap_peak = m;
        if (--cap_blocks == 0) cap_done = 1;
    }
}

static void capture_reset(void)
{
    __disable_irq();
    cap_armed  = 0;
    cap_blocks = 0;
    cap_done   = 0;
    __enable_irq();
}

static void punch_enter(void)
{
    phase   = P_WAIT_READY;
    t_phase = HAL_GetTick();
    peak    = 0;
    area    = 0;
    btn_edge = 0;                   /* ทิ้งการกดที่ค้างมาจากก่อนเข้าด่าน */
    capture_reset();
    AdcDma_Start();
    lamp_init();
    Leds_SetLevel(0);               /* หลอดวัดพลังเริ่มว่าง */
    Sound_Loop(SND_PUNCH_WAIT);     /* เริ่มหลังเสียง 0004 จบเอง ไม่ตัดคำว่า Go */
}

static int punch_score(void);

static stage_result_t punch_update(void)
{
    uint32_t now = HAL_GetTick();

    switch (phase) {

    case P_WAIT_READY:
        lamp(((now - t_phase) / BLINK_MS) % 2 == 0);   /* กะพริบเรียกให้กด */
        if (now - t_phase >= READY_TIMEOUT_MS) {
            lamp(0);
            return STAGE_DONE;                          /* คะแนน 0 */
        }
        if (ready_pressed()) {
            lamp(0);                       /* กดแล้ว ดับ */
            phase   = P_ARMED;
            t_phase = now;
            Sound_Stop();
            Screen_Draw(SC_PUNCH_HIT);     /* จอเหลืองทั้งแผ่น = ต่อยได้ */
            capture_reset();
            cap_armed = 1;                 /* นับเฉพาะหมัดหลังจากนี้ */
        }
        break;

    case P_ARMED:
        /* รอ ISR เก็บครบ 40 ms ก่อนค่อยตัดสิน (ช้ากว่าเดิมแค่ 40 ms คนไม่รู้สึก)
           พื้นที่ไม่เกิน MIN_AREA = แค่แตะหรือแผ่นสั่น ไม่นับเป็นหมัด เปิดรับใหม่ รอต่อยจริง
           */
        if (cap_done) {
            if (cap_area > MIN_AREA) {     /* หมัดจริง */
                area = cap_area;
                peak = cap_peak;
                Sound_Play(SND_PUNCH_HIT);
                Leds_SweepTo(punch_score() / 10);    /* หลอดไฟไต่ขึ้นตามแรงหมัด ได้น้อยค้างแค่เขียว ได้เยอะถึงแดง */
                phase   = P_SETTLING;
                t_phase = now;
            } else {                       /* เบาเกิน ไม่นับ เวลา 15 วิยังนับต่อจากตอนกดปุ่ม */
                capture_reset();
                cap_armed = 1;
            }
        } else if (now - t_phase >= TIMEOUT_MS && !cap_blocks) {
            capture_reset();
            return STAGE_DONE;             /* ไม่ต่อยสักที ปล่อยผ่าน คะแนน 0 */
        }
        break;

    case P_SETTLING:                       /* รู้คะแนนแล้ว รอให้กระสอบนิ่งก่อนจบด่าน */
        if (now - t_phase >= SETTLE_MS) return STAGE_DONE;
        break;
    }
    return STAGE_RUNNING;
}

/* คะแนน = 1000 x ((พื้นที่ - MIN) / (MAX - MIN)) ยกกำลัง SCORE_CURVE
   ยกกำลังน้อยกว่า 1 ยืดช่วงหมัดเบาให้ไม่กองที่ศูนย์ ที่ 0.7 พื้นที่เพิ่มทุก 0.2M คะแนนขึ้นราว 110-170 เท่า ๆ กัน
     0.4M 144 · 0.6M 310 · 0.8M 443 · 1.0M 561 · 1.2M 668 · 1.5M 818 · 1.7M 911 · 1.9M+ 1000
   (หมัดที่วัด: เบา ~170 · กลาง ~500 · แรง ~580) */
static int punch_score(void)
{
    if (area <= MIN_AREA) return 0;
    float r = (float)(area - MIN_AREA) / (float)(MAX_AREA - MIN_AREA);
    if (r >= 1.0f) return 1000;
    return (int)(1000.0f * powf(r, SCORE_CURVE) + 0.5f);
}

static void punch_render(void)
{
    /* หน้านี้ไม่มีค่าที่เปลี่ยนระหว่างเล่น
       ภาพพื้นหลังเปลี่ยนทั้งหน้าตอนกดพร้อมไปแล้วใน punch_update */
}

/* ค่าดิบของหมัดล่าสุด ใช้โชว์บนจอตอนคาลิเบรต (main.c โหมด TEST_PUNCH) */
/* slave.c เรียกเมื่อบอร์ด A ข้ามไปหน้าสรุปทั้งที่ด่านนี้ยังไม่จบ: ดับไฟปุ่ม เลิกรับหมัด */
void Punch_Abort(void)
{
    lamp(0);
    capture_reset();
}

int      Punch_RawPeak(void) { return peak; }
uint32_t Punch_RawArea(void) { return area; }

const stage_t stage_punch = {
    .name   = "PUNCH",
    .enter  = punch_enter,
    .update = punch_update,
    .score  = punch_score,
    .render = punch_render,
    .intro  = SC_INTRO_PUNCH,
    .screen = SC_PUNCH,
    .result = SC_RESULT_PUNCH,
    .countdown = COUNT_321,
};
