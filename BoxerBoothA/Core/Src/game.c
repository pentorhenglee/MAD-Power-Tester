/*
 * game.c - Core ของตู้ ดูหัวข้อใน game.h
 *
 * หน้าที่เดียวของไฟล์นี้คือ "ตอนนี้อยู่สถานะไหน และเมื่อไหร่ถึงจะเปลี่ยน"
 * มันไม่รู้เลยว่าด่านข้างในทำอะไร รู้แค่ว่าด่านคืน RUNNING หรือ DONE
 */
#include "game.h"
#include "stage.h"
#include "screens.h"
#include "hud.h"
#include "sound.h"
#include "leds.h"
#include "link.h"
#include "main.h"
#include "tim.h"
#include "ILI9341_Touchscreen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* เวลาแต่ละช่วง (มิลลิวินาที) */
#define T_NO_PERSON     30000    /* ไม่มีคนเท่านี้แล้วกลับไปหลับ */
#define T_COUNT_STEP     1000    /* นับถอยหลังหน้าละ 1 วิ ให้ตรงเสียง 0004 */
#define T_INTRO_LOCK     1200    /* หน้าอธิบายไม่รับการแตะช่วงแรก กันเสียง 0003 โดนตัด */
#define T_INTRO_LOCK_NEXT 300    /* หน้าอธิบายด่านที่ 2-3 ไม่มีเสียง 0003 ล็อกแค่กันแตะซ้ำ */
#define T_NEXT           3000    /* โชว์คะแนนด่านที่เพิ่งจบ (smash, ตะโกน) เดิม 2 วิ */
#define T_NEXT_LAST      3000    /* ด่านสุดท้าย (ต่อย) ให้ดูคะแนนต่อยกับหลอดไฟที่ไต่ขึ้นก่อนขึ้นคะแนนรวม (เดิม 0.8 วิ เร็วจนดูไม่ทัน) */
#define T_FINAL_LOCK     1500    /* หน้าคะแนนรวมค้างไว้จนกว่าจะแตะจอ แต่ไม่รับแตะช่วงแรก กันแตะติดมาจากด่านก่อน */
#define T_TAP_DEBOUNCE    300    /* แตะซ้ำเร็วกว่านี้ไม่นับ */

/* ตารางด่าน เรียงตามลำดับที่เล่นจริง
   Core วนตามตารางนี้ ไม่รู้จักชื่อด่านเลย
   อยากเพิ่มด่านที่ 4 ก็เติมอีกบรรทัดเดียว ไม่ต้องแก้อย่างอื่น */
static const stage_t *const stages[] = {
    &stage_scream,
    &stage_smash,
    &stage_punch,
};
#define STAGE_COUNT  (int)(sizeof(stages) / sizeof(stages[0]))

typedef enum {
    GS_IDLE = 0,    /* หลับ รอ PIR */
    GS_ATTRACT,     /* เรียกแขก สลับ 2 หน้า */
    GS_INTRO,       /* อธิบายด่าน รอแตะจอ */
    GS_COUNT,       /* นับถอยหลัง 3-2-1 */
    GS_RUN,         /* กำลังเล่นด่าน */
    GS_NEXT,        /* โชว์คะแนนด่านที่เพิ่งจบ */
    GS_FINAL        /* โชว์คะแนนรวม */
} game_state_t;

static game_state_t state;
static uint32_t t_enter;          /* เวลาที่เข้าสถานะปัจจุบัน */
static uint32_t t_last_person;    /* ครั้งสุดท้ายที่ PIR เจอคน */
static uint32_t t_step;           /* ใช้จับจังหวะย่อยในสถานะ */
static int  idx;                  /* ด่านที่กำลังเล่น 0..STAGE_COUNT-1 */
static int  scores[STAGE_COUNT];  /* คะแนนแต่ละด่าน */
static int  count_step;           /* เลขที่กำลังนับถอยหลัง 3,2,1 */
static volatile int flag_tap;     /* ตั้งจาก EXTI ตอนแตะจอ */
static volatile int flag_person;  /* ตั้งจาก EXTI ตอน PIR เจอคน */
static volatile uint32_t t_last_tap;

/* คะแนนรวมสูงสุดตั้งแต่เปิดเครื่อง (ไม่มีตารางอันดับ ไม่บันทึกลง Flash ปิดเครื่องแล้วหาย)
   -1 = ยังไม่มีใครเล่น จอขึ้น "---" */
static int best = -1;

/* ตัวช่วย */

static void goto_state(game_state_t s)
{
    state   = s;
    t_enter = HAL_GetTick();
    t_step  = t_enter;
}

static int  elapsed(void)       { return (int)(HAL_GetTick() - t_enter); }
static void backlight(int pct)  { __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pct * 10); }

/* วาดหน้าจอ A และสั่งจอ B ขึ้นหน้าเดียวกัน (ส่งก่อนวาด สองจอจะเปลี่ยนพร้อมกัน)
   ใช้กับหน้าระหว่างด่าน: อธิบายด่าน 3-2-1 · ระหว่างด่านกรี๊ด/ต่อย บอร์ด B วาดจอเอง ไม่ฟังคำสั่งนี้ */
static void show_both(screen_id_t s)
{
    char cmd[8];
    snprintf(cmd, sizeof cmd, "W%d", (int)s);
    Link_Send(cmd);
    Screen_Draw(s);
}

/* สิ่งที่ทำตอนเพิ่งเข้าแต่ละสถานะ */

static void enter_idle(void)
{
    goto_state(GS_IDLE);
    backlight(5);                /* หน้าเดิม แต่หรี่ไฟจนเกือบมืด */
    Link_Send("Z");              /* จอ B หรี่ตาม ตื่นเมื่อได้ A<best> จาก enter_attract */
    Sound_Stop();
    Leds_SetMode(LED_OFF);
}

static void enter_attract(void)
{
    char cmd[12];

    goto_state(GS_ATTRACT);
    t_last_person = HAL_GetTick();   /* คนเพิ่งเล่นจบยังยืนอยู่ ไม่ต้องรีบหลับ */
    backlight(100);
    Hud_Attract(best);
    snprintf(cmd, sizeof cmd, "A%d", best);
    Link_Send(cmd);                  /* จอ B กลับหน้าเรียกแขกด้วย */
    Leds_SetMode(LED_ATTRACT);
    Sound_Loop(SND_ATTRACT_LOOP);    /* รอเสียงที่เล่นอยู่ เช่นเสียงทัก ให้จบก่อนค่อยวน */
}

static void enter_intro(void)
{
    goto_state(GS_INTRO);
    show_both(stages[idx]->intro);
}

static void enter_run(void)
{
    goto_state(GS_RUN);
    /* ด่านที่จัดช่วงก่อนเริ่มเอง (COUNT_NONE = ด่านกรี๊ด) วาดหน้าของตัวเองใน enter อยู่แล้ว
       ถ้าวาดหน้าเล่นให้ก่อน จอจะแวบเป็นหน้าวงแหวนแล้วเด้งกลับหน้าอธิบาย */
    if (stages[idx]->countdown != COUNT_NONE) Screen_Draw(stages[idx]->screen);
    Leds_SetMode(LED_PLAY);
    stages[idx]->enter();
}

static void enter_count(void)
{
    if (stages[idx]->countdown == COUNT_NONE) {   /* ด่านจัดช่วงก่อนเริ่มเอง */
        enter_run();
        return;
    }
    goto_state(GS_COUNT);
    Sound_Play(SND_COUNTDOWN);                    /* 3-2-1-Go ยาว 3.4 วิ */
    count_step = 3;
    show_both(SC_READY_3);
}

static void enter_next(void)
{
    goto_state(GS_NEXT);
    scores[idx] = stages[idx]->score();

    char cmd[16];                         /* จอ B ขึ้นคะแนนด่านเดียวกัน (ด่านของ B วาดไว้แล้ว B จะข้ามเอง) */
    snprintf(cmd, sizeof cmd, "H%d %d", (int)stages[idx]->result, scores[idx]);
    Link_Send(cmd);
    Hud_Result(stages[idx]->result, scores[idx]);
    Sound_Play(SND_STAGE_CLEAR);
}

/* หน้าสรุปค้างจนกว่าจะแตะจอ ระหว่างนั้นส่ง F ซ้ำทุก FINAL_GAP_MS ไม่มีจำกัด
   บรรทัดไหนหาย รอบถัดไปจอ B ก็ขึ้นหน้าสรุปตาม (บอร์ด B ข้าม F ที่ซ้ำ ไม่วาดใหม่ จอไม่กะพริบ)
   */
#define FINAL_GAP_MS 1500
static char final_cmd[64];

static void enter_final(void)
{
    int total = 0;
    for (int i = 0; i < STAGE_COUNT; i++) total += scores[i];

    goto_state(GS_FINAL);
    /* สถิติใหม่ = ชนะคะแนนสูงสุดของคนก่อนหน้า  คนแรกที่เล่นหลังเปิดเครื่องได้เป็นสถิติแต่ไม่ฉลอง */
    int is_high = (best >= 0 && total > best);
    if (total > best) best = total;

    /* ส่งให้จอ B ก่อนวาดจอตัวเอง สองจอจะขึ้นคะแนนรวมพร้อมกัน ไม่ต้องรอจอ A วาดเสร็จ */
    snprintf(final_cmd, sizeof final_cmd, "F%d %d %d %d %d %d",   /* จอ B ขึ้นคะแนนรวมให้คนมุงเห็นด้วย */
             total, scores[0], scores[1], scores[2], best, is_high);
    Link_Send(final_cmd);
    Sound_Play(SND_GAME_OVER);       /* เพลงจบเกมเพลงเดียวทุกครั้ง ไม่แยกเสียงทำลายสถิติแล้ว */
    Leds_SetMode(LED_CELEBRATE);

    Hud_Final(total, scores, best, is_high);
}

/* ทางเข้าหลัก */

void Game_Init(void)
{
    best = -1;
    enter_attract();
}

/* ฟังบอร์ด B ตอนที่ไม่ได้อยู่ในด่าน (ระหว่างด่าน stage_remote.c อ่านเอง)
   - ทิ้งข้อความเก่าที่ค้างคิว ไม่ให้ด่านถัดไปหยิบของเก่ามาอ่าน
   - R = บอร์ด B เพิ่งบูตหรือรีเซ็ต คำสั่งที่ส่งไปก่อนหน้านั้นหายหมด
     ต้องส่งหน้าจอ ไฟ เพลง ของสถานะตอนนี้ไปให้ใหม่ */
static void listen_board_b(void)
{
    char line[64];
    while (Link_Receive(line, sizeof line)) {
        if (line[0] != 'R') continue;
        if (state == GS_ATTRACT) {
            enter_attract();
        } else if (state == GS_FINAL) {
            Link_Send(final_cmd);    /* B บูตใหม่ตอนอยู่หน้าสรุป ส่งคะแนนรวมให้อีกรอบ */
            Leds_SetMode(LED_CELEBRATE);
        } else if (state == GS_IDLE) {
            Link_Send("Z");          /* B บูตใหม่ไฟจอเต็ม ต้องสั่งหรี่อีกรอบ */
            Sound_Stop();
            Leds_SetMode(LED_OFF);
        } else {
            Leds_SetMode(LED_PLAY);
        }
    }
}

void Game_Tick(void)
{
    /* สำรอง EXTI: อ่านขา T_IRQ ตรง ๆ ทุกรอบด้วย ถ้า EXTI พลาดขอบ (หน้าสัมผัสเด้ง / นิ้ววางค้าง)
       ตอนเปลี่ยนจากไม่แตะเป็นแตะก็ยังนับ ใช้ debounce ตัวเดียวกับ EXTI จึงไม่นับซ้ำ
       */
    static int tp_was = 0;
    int tp = (TP_Touchpad_Pressed() == TOUCHPAD_PRESSED);
    if (tp && !tp_was) Game_OnTap();
    tp_was = tp;

    int tapped = flag_tap;
    flag_tap = 0;
    if (flag_person) { t_last_person = HAL_GetTick(); flag_person = 0; }

    if (state != GS_RUN) listen_board_b();

    switch (state) {

    case GS_IDLE:
        /* PIR เจอคน หรือมีคนแตะจอ ก็ตื่น · แตะจอไว้กันเหนียวถ้า PIR เสียหรือหลุด เกมจะไม่หลับตาย */
        if (tapped || HAL_GetTick() - t_last_person < 1000) {
            Sound_Play(SND_PERSON);
            enter_attract();                            /* เพลงวนจะรอเสียงทักจบก่อน */
        }
        break;

    case GS_ATTRACT:
        if (tapped) {
            Sound_Play(SND_START);
            idx = 0;
            for (int i = 0; i < STAGE_COUNT; i++) scores[i] = 0;
            enter_intro();
            break;
        }
        if (HAL_GetTick() - t_last_person >= T_NO_PERSON) enter_idle();
        break;

    case GS_INTRO:
        /* เสียง 0003 "Ready... Fight!" ยาว 1.1 วิ ถ้าแตะเริ่มด่านเร็วกว่านั้น
           เสียงนับถอยหลังจะตัดคำว่า Fight ทิ้ง เลยไม่รับการแตะในช่วงแรก
           ผลพลอยได้คือกันแตะรัวจนข้ามหน้าอธิบายไปด้วย */
        /* ล็อกเต็ม 1.2 วิเฉพาะด่านแรกที่มีเสียง 0003 ด่านถัดไปไม่มีเสียงนี้ ล็อกสั้นพอกันแตะซ้ำจากหน้าคะแนน
           (เดิมล็อก 1.2 วิทุกด่าน รวมกับหน้าคะแนน 2 วิ แตะไม่ติดนานจนคนนึกว่าค้าง) */
        if (tapped && elapsed() >= (idx == 0 ? T_INTRO_LOCK : T_INTRO_LOCK_NEXT)) enter_count();
        break;

    case GS_COUNT:
        if (HAL_GetTick() - t_step >= T_COUNT_STEP) {
            t_step = HAL_GetTick();
            count_step--;
            if (count_step <= 0) enter_run();          /* จังหวะนี้ตรงกับคำว่า "Go" พอดี */
            else show_both(count_step == 2 ? SC_READY_2 : SC_READY_1);
        }
        break;

    case GS_RUN:
        if (stages[idx]->update() == STAGE_DONE) enter_next();
        else                                     stages[idx]->render();
        break;

    case GS_NEXT:
        if (elapsed() >= (idx == STAGE_COUNT - 1 ? T_NEXT_LAST : T_NEXT)) {
            idx++;
            if (idx >= STAGE_COUNT) enter_final();
            else                    enter_intro();
        }
        break;

    case GS_FINAL:
        if (HAL_GetTick() - t_step >= FINAL_GAP_MS) {   /* ย้ำหน้าสรุปให้จอ B */
            t_step = HAL_GetTick();
            Link_Send(final_cmd);
        }
        /* ค้างหน้าคะแนนรวมไว้จนกว่าจะแตะจอ (เดิมกลับหน้าเรียกแขกเองหลัง 15 วิ) */
        if (tapped && elapsed() >= T_FINAL_LOCK) enter_attract();
        break;
    }
}

void Game_OnTap(void)
{
    uint32_t now = HAL_GetTick();
    if (now - t_last_tap < T_TAP_DEBOUNCE) return;   /* นิ้วเดียวแตะทีเดียว ไม่ใช่สองที */
    t_last_tap = now;
    flag_tap = 1;
}

void Game_OnPerson(void) { flag_person = 1; }

/* EXTI ทุกเส้นวิ่งมาที่ฟังก์ชันเดียวกัน แยกด้วยหมายเลขขา */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == T_IRQ_Pin) Game_OnTap();
#ifdef PIR_OUT_Pin
    if (pin == PIR_OUT_Pin) Game_OnPerson();
#endif
}
