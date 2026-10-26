/*
 * stage_scream.c - ด่านที่ 1 กรี๊ด (บอร์ด B: ขา PA3 / ADC1_IN3 + DMA Circular)
 *
 * คนทำ: กัน
 *
 * กติกาของด่าน (โมเดล A สะสมพลังงานเสียง จาก MAD_Project_Test_Mic):
 *   1. คำนวณ RMS ของคลื่นเสียงใน DMA Half/Full Callback (หักไฟตรง DC ~1.25V ออกก่อน)
 *   2. ช่วงเงียบ 2 วิ (PH_QUIET) วัดค่าเฉลี่ยเสียงพื้นห้อง (noise_floor)
 *   3. ช่วงตะโกน 10 วิ (PH_RUN) ต้องลากเสียงเกินเกณฑ์ติดต่อกัน >= 400 ms ถึงเริ่มบวกแต้ม
 *      (กรองคำพูดสั้นๆ ออก) และหยุดหายใจสั้นๆ ไม่เกิน 250 ms ยังลากต่อได้โดยไม่ล้างแต้มเดิม
 *   4. ส่ง V0 / V1 / V2 / V3 ให้จอบอร์ด A ในห้องมืดวาดตามจังหวะจริง
 */
#include "stage.h"
#include "hud.h"
#include "sound.h"
#include "leds.h"
#include "main.h"
#include "adc.h"
#include "link.h"
#include "adc_dma.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PROMPT_MIN_MS   4000     /* เสียงเกริ่น 2.3 วิ + เวลาหันไปปิดม่าน */
#define PROMPT_MAX_MS   6000     /* รอนานสุด กันค้างถ้าโมดูลเสียงไม่ตอบ */
#define QUIET_MS        2000     /* ช่วงเงียบวัดเสียงพื้น */
#define RUN_MS         10000     /* ช่วงตะโกน */

/* ค่าคงที่จาก MAD_Project_Test_Mic (คูณ 10 เพื่อเก็บความละเอียดทศนิยม 1 ตำแหน่งแบบจำนวนเต็ม) */
#define ELEC_NOISE_RMS    55U    /* สัญญาณรบกวนไฟฟ้าพื้นฐานตอนห้องเงียบ */
#define LEVEL_BASE      40.0f
#define LEVEL_SCALE      1.45f

#define NOISE_MARGIN     180     /* 18.0 dB เหนือพื้นห้อง ถึงนับว่าเริ่มออกเสียงตะโกน */
#define EXCESS_CAP       350     /* 35.0 dB เพดานกันคนเอาปากจ่อไมค์ */
#define MIN_RUN_MS       400U    /* ต้องลากเสียงติดต่อกัน >= 400 ms ถึงเริ่มบวกพลังงาน */
#define GRACE_MS         250U    /* ผ่อนผันหยุดหายใจสั้นๆ ไม่เกิน 250 ms */
#define MAX_ENERGY      2000     /* พลังงานอ้างอิงเต็ม 1000 คะแนน (= 200.0 dB*s) เดิม 2900 คนทั่วไปได้แค่ 550-600 */
#define LEVEL_FULL       500     /* 50.0 dB เหนือพื้นห้อง = ไฟเส้นและกราฟคลื่นสูง 100% */

typedef enum { PH_PROMPT = 0, PH_QUIET, PH_RUN } phase_t;

static phase_t  phase;
static uint32_t t_phase;
static uint32_t t_last;
static uint32_t silence_ms;
static long     energy_ms;       /* สะสมเป็นหน่วย (ความดัง × มิลลิวินาที) */
static long     noise_sum;
static int      noise_n;
static int      noise_floor = 400;
static int      level       = 400;
static int      level_pct   = 0;
static int      quiet_step;
static int      last_score  = -1;

/* ตัวแปรเสริมสำหรับโหมดเทสต์ไมค์ออก Serial Monitor ใน main.c */
volatile uint32_t scream_raw_rms = 0;
volatile uint32_t scream_raw_mean = 0;   /* ค่าเฉลี่ย DC ของขา PA3 ไว้เช็กสาย: ไมค์ปกติ ~1550 (1.25V) */
int               scream_dbg_lvl = 400;
int               scream_dbg_flr = 400;
uint32_t          scream_run_ms  = 0;

/* รวมโค้ด 25 ต.ค.: ADC1 + DMA ย้ายไปอยู่ adc_dma.c ที่เดียว (ด่านต่อยใช้ร่วมกัน)
   adc_dma.c ส่งข้อมูลมาทีละ 128 คู่ (~5 ms) แต่สูตรเดิมของกันใช้ไมค์ 512 จุด (~20 ms)
   เลยสะสมไว้ 4 ก้อนก่อน แล้วค่อยคิด RMS ผลเท่าสูตรเดิม (ลบค่าเฉลี่ย DC ออกก่อนเหมือนกัน) */
#define RMS_SAMPLES  512

static uint32_t acc_n;
static uint32_t acc_sum;
static uint64_t acc_sq;

/* adc_dma.c เรียกจาก ISR ทุกครึ่งบัฟเฟอร์ p[2*i] = ไมค์ PA3 */
void Scream_OnAdcBlock(const uint16_t *p, int pairs)
{
    for (int i = 0; i < pairs; i++) {
        uint32_t v = p[2 * i];
        acc_sum += v;
        acc_sq  += (uint64_t)v * v;
    }
    acc_n += pairs;

    if (acc_n >= RMS_SAMPLES) {
        double mean = (double)acc_sum / acc_n;
        double var  = (double)acc_sq / acc_n - mean * mean;   /* = ค่าเฉลี่ยของ (x - mean)^2 */
        scream_raw_rms = (var > 0) ? (uint32_t)sqrt(var) : 0;
        scream_raw_mean = (uint32_t)mean;
        acc_n = 0; acc_sum = 0; acc_sq = 0;
    }
}

/* ============================================================
 *  อ่านความดังจากไมค์
 *  คืนค่าดัชนีความดังแบบจำนวนเต็มคูณ 10 (เช่น 400..1050 แทน 40.0..105.0 dB)
 * ============================================================ */
static int mic_level(void)
{
    AdcDma_Start();
    uint32_t raw = scream_raw_rms;
    uint32_t net = (raw > ELEC_NOISE_RMS) ? (raw - ELEC_NOISE_RMS) : 0U;
    float gain = LEVEL_SCALE * 20.0f * log10f(1.0f + ((float)net / 25.0f))
                 + ((float)net * 0.012f);
    scream_dbg_lvl = (int)((LEVEL_BASE + gain) * 10.0f);
    return scream_dbg_lvl;
}

static void scream_enter(void)
{
    AdcDma_Start();
    phase         = PH_PROMPT;
    t_phase       = HAL_GetTick();
    energy_ms     = 0;
    scream_run_ms = 0;
    silence_ms    = 0;
    level_pct     = 0;
    last_score    = -1;

    Screen_Draw(SC_INTRO_SCREAM);            /* ข้อความบนจอตรงกับเสียงเกริ่น */
    Sound_Play(SND_SCREAM_PROMPT);
    Link_Send("V0");                         /* บอกบอร์ด A: ขึ้นหน้าอธิบายด่าน */
    Leds_SetMode(LED_WAIT);                  /* แดงจาง = ยังห้ามตะโกน */
    Leds_SetLevel(0);
}

static stage_result_t scream_update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t age = now - t_phase;

    switch (phase) {

    case PH_PROMPT:                          /* รอเสียงเกริ่นจบจริง */
        if ((age >= PROMPT_MIN_MS && !Sound_IsBusy()) || age >= PROMPT_MAX_MS) {
            phase      = PH_QUIET;
            t_phase    = now;
            noise_sum  = 0;
            noise_n    = 0;
            quiet_step = 2;
            Screen_Draw(SC_QUIET_2);
            Link_Send("V1");
        }
        break;

    case PH_QUIET:                           /* เงียบ 2 วิ เก็บค่าเฉลี่ยเสียงพื้นห้อง */
        noise_sum += mic_level();
        noise_n++;
        if (quiet_step == 2 && age >= QUIET_MS / 2) {
            quiet_step = 1;
            Screen_Draw(SC_QUIET_1);
            Link_Send("V2");
        }
        if (age >= QUIET_MS) {
            noise_floor = noise_n ? (int)(noise_sum / noise_n) : 400;
            if (noise_floor < 400) noise_floor = 400;
            if (noise_floor > 720) noise_floor = 720;
            scream_dbg_flr = noise_floor;

            phase      = PH_RUN;
            t_phase    = now;
            t_last     = now;
            last_score = 0;
            Screen_Draw(SC_SCREAM);
            Hud_ScreamReset();               /* ลบเลขตัวอย่างในภาพ วงแหวนว่าง 0 คะแนน 10.0 วิ */
            Leds_SetMode(LED_PLAY);          /* ไฟเปลี่ยน = ตะโกนได้ */
            Link_Send("V3 0 10000 0");
        }
        break;

    case PH_RUN: {
        int dt = (int)(now - t_last);
        t_last = now;
        level  = mic_level();

        int threshold = noise_floor + NOISE_MARGIN;
        if (level >= threshold) {
            silence_ms     = 0;
            scream_run_ms += (uint32_t)dt;
            if (scream_run_ms >= MIN_RUN_MS) {
                int excess = level - threshold;
                if (excess > EXCESS_CAP) excess = EXCESS_CAP;
                energy_ms += (long)excess * dt;
            }
        } else {
            silence_ms += (uint32_t)dt;
            if (silence_ms >= GRACE_MS) {
                scream_run_ms = 0;           /* ล้างแค่ตัวนับลากเสียง ไม่ล้าง energy_ms */
            }
        }

        int above = level - noise_floor;
        int pct   = above > 0 ? above * 100 / LEVEL_FULL : 0;
        level_pct = pct > 100 ? 100 : pct;
        Leds_SetLevel(level_pct);

        if (age >= RUN_MS) {
            Leds_SetLevel(0);
            return STAGE_DONE;
        }
        break;
    }
    }
    return STAGE_RUNNING;
}

static int scream_score(void)
{
    long s = energy_ms / MAX_ENERGY;
    if (s > 1000) s = 1000;
    if (s < 0)    s = 0;
    return (int)s;
}

static void scream_render(void)
{
    static uint32_t t_draw;

    if (phase != PH_RUN) return;
    /* วาดไม่เกิน 10 ครั้งต่อวินาที พอให้ตาเห็นว่าขยับ และไม่กินเวลา SPI */
    if (HAL_GetTick() - t_draw < 100) return;
    t_draw = HAL_GetTick();

    int s = scream_score();
    if (s != last_score) {
        last_score = s;
        Hud_ScreamScore(s);
        Hud_ScreamArc(s);                    /* วงแหวนเต็ม = 1000 คะแนน */
    }
    int left = RUN_MS - (int)(HAL_GetTick() - t_phase);
    Hud_ScreamTime(left);
    Hud_ScreamWave(level_pct);

    char msg[32];                            /* ส่งค่าเดียวกันให้จอบอร์ด A ในห้อง */
    snprintf(msg, sizeof msg, "V3 %d %d %d", s, left < 0 ? 0 : left, level_pct);
    Link_Send(msg);
}

const stage_t stage_scream = {
    .name      = "SCREAM",
    .enter     = scream_enter,
    .update    = scream_update,
    .score     = scream_score,
    .render    = scream_render,
    .intro     = SC_INTRO_SCREAM,
    .screen    = SC_SCREAM,
    .result    = SC_RESULT_SCREAM,
    .countdown = COUNT_NONE,                 /* ด่านนี้จัดช่วงก่อนเริ่มเอง */
};
