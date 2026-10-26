/*
 * stage_smash.c - ด่านที่ 2 ตบปุ่ม
 *
 * คนทำ: ICHIZXC
 *
 * กติกาของด่าน: ปุ่ม 9 ดวงเรียง 3x3 ไฟติดพร้อมกัน LIT_COUNT ดวงตลอดด่าน
 *               ตบดวงที่ติด -> ดวงนั้นดับ แล้วไปติดดวงอื่นแทน (อีกสองดวงอยู่ที่เดิม)
 *               ตบให้ได้มากที่สุดใน 30 วินาที ไม่จำกัดจำนวนครั้ง
 * คะแนน (0-1000 เพดานเท่าด่านอื่น):
 *   ตบถูก  = ได้แต้มตามอายุของดวงนั้น นับจากดวงนั้นติดถึงตอนตบ
 *            เร็ว <= 0.35 วิ ได้ 8 · ช้า >= 1.5 วิ ได้ 2 · ระหว่างนั้นลดลงเป็นเส้นตรง
 *   ตบผิด  = หัก 15 ทุกปุ่มที่ผิด (ตบมั่วทั้งแผงจึงขาดทุน) ปุ่มที่ผิดกะพริบขาวบนจอ
 *   คะแนนไม่ติดลบ และไม่เกิน SCORE_MAX
 *
 * ภาพรวม
 *   TIM6 ขัดจังหวะทุก 1 ms -> อ่านปุ่มทั้ง 9 + กันเด้ง -> ตั้งธง "ปุ่มนี้เพิ่งถูกกด"
 *   update() -> หยิบธงมาดู ตบถูกได้แต้มแล้วย้ายดวงนั้น ตบผิดหักแต้ม -> ครบ 30 วิ จบ
 *   render() -> Hud_Smash ใน hud.c วาดเฉพาะที่เปลี่ยน + ส่งข้อความ M ให้จอ B วาดด้วยตัวเดียวกัน
 *   สีบนจอตรงกับปุ่มจริง: แถวบนแดง แถวกลางเหลือง แถวล่างน้ำเงิน (ดับ = สีเดิมแบบมืด)
 *
 *  ดวงที่ติดเก็บเป็น "mask" ตัวเลขตัวเดียว บิต i = ดวง i ติดอยู่
 *  เช่น ดวง 0, 4, 8 ติด = บิต 0, 4, 8 = 0x111 = 273
 */
#include "stage.h"
#include "hud.h"                /* Hud_Smash - ตัววาดร่วมของจอ A และจอ B */
#include "link.h"               /* ส่งค่าให้จอ B วาดตาม */
#include "main.h"
#include "sound.h"
#include "tim.h"                /* htim6 */
#include <stdlib.h>             /* rand(), srand() */
#include <stdio.h>              /* snprintf */

#define RUN_MS      30000       /* เล่น 30 วินาที */
#define DEBOUNCE_MS    20       /* ต้องอ่านได้ "กด" ติดกันกี่ ms ถึงนับ (TIM6 อ่านทุก 1 ms) */
#define LIT_COUNT       3       /* ไฟติดพร้อมกันกี่ดวง */

/* คะแนน - ปรับตัวเลขตรงนี้หลังเทสกับคนเล่นจริง (ทดลองใน Project_Switch ก่อน)
   ชุดแรก 12/4 แต้ม 500/2000 ms หัก 10 -> คนเล่นจริงแตะ 1000 ทุกรอบ จึงลดลงประมาณครึ่ง */
#define PTS_FAST        8       /* ตบภายใน FAST_MS หลังดวงนั้นติด ได้แต้มเต็ม */
#define PTS_SLOW        2       /* ช้ากว่า SLOW_MS ได้แต้มขั้นต่ำ */
#define FAST_MS       350
#define SLOW_MS      1500       /* ระหว่าง FAST_MS ถึง SLOW_MS แต้มลดลงเป็นเส้นตรง */
#define PENALTY        15       /* ตบผิดหักเท่านี้ ต่อหนึ่งปุ่ม */
#define SCORE_MAX    1000       /* เพดานเท่าด่านอื่น คะแนนรวมกับตารางอันดับจะได้ไม่เพี้ยน */
#define WRONG_FLASH_MS 150      /* ปุ่มที่ตบผิดเป็นสีขาวบนจอนานเท่านี้ */

static uint32_t t_start;
static uint16_t lit_mask;       /* บิต i = ดวง i ติดอยู่ */
static uint32_t t_lit[9];       /* เวลาที่แต่ละดวงเริ่มติด ใช้วัดความเร็ว */
static int score;               /* 0..SCORE_MAX */
static uint16_t wrong_mask;     /* บิต i = ดวง i เพิ่งตบผิด (กะพริบขาวบนจอ) */
static uint32_t t_wrong;

/* ============================================================
 *  1 - เปิด/ปิดไฟในปุ่ม
 *  ไฟ 9 ดวงอยู่ที่ PE7 PE8 PE10 PE11 PE12 PE13 PE14 PE15 PF0
 *  (ข้าม PE9 เพราะเป็นแบ็คไลท์จอ)  ใช้ชื่อขาที่ CubeMX ตั้งไว้ใน main.h
 * ============================================================ */
static GPIO_TypeDef *const LAMP_PORT[9] = {
    LAMP_1_GPIO_Port, LAMP_2_GPIO_Port, LAMP_3_GPIO_Port,
    LAMP_4_GPIO_Port, LAMP_5_GPIO_Port, LAMP_6_GPIO_Port,
    LAMP_7_GPIO_Port, LAMP_8_GPIO_Port, LAMP_9_GPIO_Port,
};
static const uint16_t LAMP_PIN[9] = {
    LAMP_1_Pin, LAMP_2_Pin, LAMP_3_Pin,
    LAMP_4_Pin, LAMP_5_Pin, LAMP_6_Pin,
    LAMP_7_Pin, LAMP_8_Pin, LAMP_9_Pin,
};

static void lamp(int n, int on)
{
    HAL_GPIO_WritePin(LAMP_PORT[n], LAMP_PIN[n], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================
 *  2 - อ่านว่าปุ่มดวงไหนถูกกด  คืน 0-8 หรือ -1
 *
 *  อ่านทั้งพอร์ต D ทีเดียว (มองพอร์ตเป็นบัสขนาน):
 *     บิต 0-7 = ปุ่ม 1-8 (PD0-PD7),  บิต 14 = ปุ่ม 9 (PD14)
 *     pull-up -> ปล่อย = 1, กด = 0  จึงต้องกลับบิต (~)
 *
 *  TIM6 เรียก Buttons_Scan1ms() ทุก 1 ms:
 *     กดอยู่  -> นับ +1   พอนับถึง DEBOUNCE_MS พอดี = กดจริง ตั้งธงของปุ่มนั้น
 *     ปล่อย   -> นับกลับเป็น 0
 *  นับถึง 20 ได้ครั้งเดียวต่อการกดหนึ่งครั้ง จึงกดค้างแล้วไม่นับซ้ำ
 *  noise/เด้งสั้น ๆ ไม่ถึง 20 ms ก็ไม่นับ
 * ============================================================ */
static uint16_t read_buttons(void)          /* คืนบิต 0-8 = ปุ่ม 1-9, 1 = กดอยู่ */
{
    uint16_t idr = (uint16_t)~GPIOD->IDR;   /* กลับบิต: กด = 1 */
    uint16_t b = idr & 0x00FF;              /* ปุ่ม 1-8 */
    if (idr & (1u << 14)) b |= (1u << 8);   /* ปุ่ม 9 ย้ายมาไว้บิต 8 */
    return b;
}

static uint8_t           hold_ms[9];        /* แต่ละปุ่มกดค้างมากี่ ms แล้ว */
static volatile uint16_t pressed_flags;     /* บิต i = ปุ่ม i เพิ่งถูกกด (ISR ตั้ง, update ล้าง) */

static void Buttons_Scan1ms(void)
{
    uint16_t b = read_buttons();
    for (int i = 0; i < 9; i++) {
        if (b & (1u << i)) {
            if (hold_ms[i] < 255) hold_ms[i]++;
            if (hold_ms[i] == DEBOUNCE_MS) pressed_flags |= (1u << i);
        } else {
            hold_ms[i] = 0;
        }
    }
}

/* HAL เรียกฟังก์ชันนี้ทุกครั้งที่ timer ใด ๆ นับครบรอบ เช็กว่าเป็น TIM6 ก่อน */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) Buttons_Scan1ms();
}

/* คืนทุกปุ่มที่เพิ่งถูกกดตั้งแต่ครั้งก่อน บิต i = ปุ่ม i
   (เดิมคืนแค่ดวงเลขน้อยสุด ถ้าฝ่ามือโดนดวงที่ติดพร้อมดวงข้าง ๆ จะนับดวงข้าง ๆ แทน) */
static uint16_t buttons_pressed(void)
{
    __disable_irq();                        /* อ่านแล้วล้างธง โดยไม่ให้ ISR แทรกตรงกลาง */
    uint16_t f = pressed_flags;
    pressed_flags = 0;
    __enable_irq();
    return f;
}

/* ============================================================
 *  3 - ติดไฟดวงใหม่ 1 ดวง แบบสุ่ม
 *  สุ่ม 0-8 ไปเรื่อย ๆ จนได้ดวงที่ "ยังไม่ติด" และ "ไม่ใช่ดวงที่เพิ่งตบ"
 *  (ไม่ให้ไปติดที่เดิมทันที คนเล่นจะตบที่เดิมรัว ๆ ไม่ได้)
 *  ไฟติด 3 ดวง + ดวงที่เพิ่งตบ = เหลือให้สุ่มอย่างน้อย 5 ดวง สุ่มไม่กี่รอบก็เจอ
 * ============================================================ */
static void light_new(uint16_t avoid)
{
    int n;
    do {
        n = rand() % 9;
    } while ((lit_mask | avoid) & (1u << n));

    lamp(n, 1);
    lit_mask |= (1u << n);
    t_lit[n] = HAL_GetTick();
}

/* แต้มของการตบถูกหนึ่งครั้ง ตามอายุของดวงนั้น (ms ตั้งแต่ดวงนั้นติดจนตบ) */
static int hit_points(uint32_t ms)
{
    if (ms <= FAST_MS) return PTS_FAST;
    if (ms >= SLOW_MS) return PTS_SLOW;
    return PTS_FAST - (PTS_FAST - PTS_SLOW) * (int)(ms - FAST_MS) / (SLOW_MS - FAST_MS);
}

static int sent_lit, sent_score, sent_tenths, sent_wrong;   /* ค่าที่ส่งให้จอ B ล่าสุด */

static void smash_enter(void)
{
    score      = 0;
    wrong_mask = 0;
    t_start    = HAL_GetTick();
    srand(t_start);                     /* เวลาที่คนแตะเริ่มเกมไม่เคยเท่ากัน = สุ่มไม่ซ้ำทุกตา */

    for (int i = 0; i < 9; i++) hold_ms[i] = 0;
    pressed_flags = 0;
    HAL_TIM_Base_Start_IT(&htim6);      /* เริ่มสแกนปุ่มทุก 1 ms */

    Hud_SmashReset();                   /* game.c วาดภาพ SC_SMASH ให้แล้ว เฟรมแรกวาดปุ่ม 9 ดวง + ป้าย SCORE */

    for (int i = 0; i < 9; i++) lamp(i, 0);
    lit_mask = 0;
    for (int k = 0; k < LIT_COUNT; k++) light_new(0);
    Sound_Loop(SND_SMASH_BGM);      /* เริ่มหลังเสียง 0004 จบเอง ไม่ตัดคำว่า Go */

    char cmd[8];                    /* จอ B ขึ้นหน้าตบปุ่มด้วย แล้ว smash_render ส่งค่าสดตามไป */
    snprintf(cmd, sizeof cmd, "W%d", (int)SC_SMASH);
    Link_Send(cmd);
    sent_lit = sent_score = sent_tenths = sent_wrong = -2;
}

static stage_result_t smash_update(void)
{
    uint32_t now  = HAL_GetTick();
    uint16_t f    = buttons_pressed();
    uint16_t hit  = f &  lit_mask;      /* ปุ่มที่กดและไฟติดอยู่ = ตบถูก */
    uint16_t miss = f & ~lit_mask;      /* ปุ่มที่กดแต่ไฟไม่ติด = ตบผิด */

    /* แยก hit / miss จาก lit_mask ก่อนติดดวงใหม่ ดวงใหม่ที่บังเอิญถูกกดอยู่ในรอบเดียวกันจะไม่ได้แต้มฟรี */
    for (int i = 0; i < 9; i++) {
        if (hit & (1u << i)) {
            score += hit_points(now - t_lit[i]);
            if (score > SCORE_MAX) score = SCORE_MAX;
            lamp(i, 0);
            lit_mask &= ~(1u << i);
        }
        if (miss & (1u << i)) {
            score -= PENALTY;
            if (score < 0) score = 0;
        }
    }
    for (int i = 0; i < 9; i++)         /* ตบถูกไปกี่ดวง ติดดวงใหม่เท่านั้น ไม่ติดที่เดิม */
        if (hit & (1u << i)) light_new(hit);

    if (miss) { wrong_mask = miss; t_wrong = now; }
    if (now - t_wrong >= WRONG_FLASH_MS) wrong_mask = 0;

    if (now - t_start >= RUN_MS) {
        for (int i = 0; i < 9; i++) lamp(i, 0);     /* ดับไฟทั้งหมดตอนจบ */
        HAL_TIM_Base_Stop_IT(&htim6);               /* เลิกสแกนปุ่ม */
        return STAGE_DONE;
    }
    return STAGE_RUNNING;
}

static int smash_score(void)
{
    return score;
}

/* ============================================================
 *  4 - วาดจอ A และส่งค่าเดียวกันให้จอ B วาดตาม
 *  ตัววาดอยู่ใน hud.c (Hud_Smash) ใช้ร่วมกันทั้งสองบอร์ด จอ A กับจอ B จึงหน้าตาเหมือนกันเสมอ
 *  ปุ่มสีตามแถวให้ตรงกับปุ่มจริง · ปุ่มที่ตบผิดเป็นสีขาว WRONG_FLASH_MS · คะแนนสด · เวลา · แถบเวลา
 * ============================================================ */
static void smash_render(void)
{
    uint32_t used = HAL_GetTick() - t_start;
    int remain_ms = (used >= RUN_MS) ? 0 : (int)(RUN_MS - used);

    /* จอ B: ส่งเฉพาะตอนมีอะไรเปลี่ยน ~10 ครั้ง/วิ (เวลาเปลี่ยนทุก 0.1 วิ)
       */
    int tenths = remain_ms / 100;
    if (lit_mask != sent_lit || score != sent_score || tenths != sent_tenths || wrong_mask != sent_wrong) {
        sent_lit = lit_mask;  sent_score = score;  sent_tenths = tenths;  sent_wrong = wrong_mask;
        char cmd[32];
        snprintf(cmd, sizeof cmd, "M%d %d %d %d", lit_mask, score, remain_ms, wrong_mask);
        Link_Send(cmd);
    }

    Hud_Smash(lit_mask, wrong_mask, score, remain_ms);
}

const stage_t stage_smash = {
    .name   = "SMASH",
    .enter  = smash_enter,
    .update = smash_update,
    .score  = smash_score,
    .render = smash_render,
    .intro  = SC_INTRO_SMASH,
    .screen = SC_SMASH,
    .result = SC_RESULT_SMASH,
    .countdown = COUNT_321,
};
