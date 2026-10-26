/*
 * hud.c - ดูหัวข้อใน hud.h
 *
 * พิกัดทุกตัวในไฟล์นี้มาจาก tools/screens/build_screens.py ซึ่งตรงกับดีไซน์ Figma
 * ถ้าแก้ดีไซน์แล้วตำแหน่งตัวเลขย้าย ต้องแก้ตรงนี้ตามด้วย
 */
#include "hud.h"
#include "img.h"
#include "digits.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define C_BLACK   0x0000
#define C_WHITE   0xF7BE      /* #f5f5f0 */
#define C_YELLOW  0xF664      /* #f4ce20 */
#define C_CYAN    0x263D      /* #27c7e8 */
#define C_TRACK   0x09A8      /* #0b3540 สีรางวงแหวนตอนยังว่าง */

/* เลขชิดขวาที่ขอบ xr กว้าง width ช่อง
   ล้างกว้างกว่าช่องข้างละ 3 จุด เพราะเลขตัวอย่างในภาพวางตำแหน่งแบบมีทศนิยม
   ขอบตัวอักษรเลยล้นออกมาเกินช่องของเรานิดหน่อย ถ้าไม่ล้างจะเห็นเป็นขีดบาง ๆ */
static void num_right(const digit_font_t *f, int xr, int y, int value, int width,
                      uint16_t fg)
{
    char s[12];
    IMG_FillRect(xr - width * f->w - 3, y, width * f->w + 6, f->h, C_BLACK);
    snprintf(s, sizeof s, "%*d", width, value);
    int len = (int)strlen(s);
    IMG_DrawNumber(f, xr - len * f->w, y, s, fg, C_BLACK);
}

/* เลขกึ่งกลางกล่อง ล้างทั้งกล่องก่อน เพราะเลขตัวอย่างในภาพอาจยาวกว่าเลขจริง */
static void num_center(const digit_font_t *f, int x, int w, int y, int value,
                       uint16_t fg)
{
    char s[12];
    snprintf(s, sizeof s, "%d", value);
    int len = (int)strlen(s);
    IMG_FillRect(x, y, w, f->h, C_BLACK);
    IMG_DrawNumber(f, x + (w - len * f->w) / 2, y, s, fg, C_BLACK);
}

/* หน้าทั่วไป */

/* แถบล่างของหน้าแรกและหน้าสรุป (เส้นคั่น, HIGH SCORE / ALL TIME BEST / NEW HIGH SCORE, มงกุฎ, ตัวเลข)
   อยู่ในภาพที่อบไว้ ทาสีพื้นหลังทับให้หายไป ไม่ต้องแปลงภาพใหม่
   พิกัดวัดจาก tools/screens/fm_*.png  กล่องไม่แตะสายฟ้าหรือกรอบจอ
   */
#define ATTRACT_BOTTOM_X   44, 248, 153, 60
#define FINAL_BOTTOM_X     20, 248, 200, 58

void Hud_Attract(int best)
{
    (void)best;                                              /* ไม่โชว์คะแนนสูงสุดแล้ว */
    Screen_Draw(SC_ATTRACT);
    IMG_FillRect(ATTRACT_BOTTOM_X, C_BLACK);
}

void Hud_Result(screen_id_t screen, int score)
{
    Screen_Draw(screen);
    num_center(&digits48, 48, 144, 128, score, C_WHITE);    /* ในกรอบ SCORE */
}

void Hud_Final(int total, const int stage[3], int best, int is_high)
{
    Screen_Draw(is_high ? SC_FINAL_HIGH : SC_FINAL);
    num_center(&digits48, 48, 144, 60, total, is_high ? C_YELLOW : C_WHITE);
    for (int i = 0; i < 3; i++)                              /* คะแนนรายด่าน */
        num_right(&digits16, 214, 159 + i * 30, stage[i], 4, C_WHITE);
    (void)best;
    IMG_FillRect(FINAL_BOTTOM_X, C_BLACK);                   /* ลบ ALL TIME BEST / NEW HIGH SCORE */
}

/* ด่านกรี๊ด
   วงแหวนศูนย์กลาง (120,132) รัศมี 56 หนา 12 เริ่มที่ 120 องศา กวาดตามเข็ม 300 องศา
   เว้นช่องว่างไว้ด้านล่าง ภาพที่อบไว้มีส่วนที่เติมแล้ว 62% เป็นตัวอย่าง ต้องทาทับก่อนเล่น */

#define ARC_CX    120
#define ARC_CY    132
#define ARC_R      56
#define ARC_HALF    6
#define ARC_START 120
#define ARC_SPAN  300

static int arc_done = -1;          /* เติมไปแล้วถึงองศาที่เท่าไหร่ (-1 = ยังไม่เติมเลย) */

#define WAVE_N 33                  /* คลื่นเสียง 33 แท่ง แท่งละกว้าง 3 ห่างกัน 6 */
static uint8_t wave_h[WAVE_N];     /* ความสูงที่ควรเป็น */
static uint8_t wave_drawn[WAVE_N]; /* ความสูงที่วาดอยู่บนจอจริงตอนนี้ */

static void arc_dot(int deg, uint16_t c)
{
    double rad = (ARC_START + deg) * 3.14159265 / 180.0;
    IMG_FillCircle(ARC_CX + (int)lround(ARC_R * cos(rad)),
                   ARC_CY + (int)lround(ARC_R * sin(rad)), ARC_HALF, c);
}

void Hud_ScreamBlank(void)
{
    for (int a = 0; a <= ARC_SPAN; a += 2) arc_dot(a, C_TRACK);   /* วงแหวนว่าง */
    arc_done = -1;
    IMG_DrawNumber(&digits40, 84, 108, "   ", C_WHITE, C_BLACK);   /* คะแนน */
    IMG_FillRect(68, 264, 82, 32, C_BLACK);                        /* เวลา ถึงหน้าตัว s */
    IMG_FillRect(21, 214, 198, 34, C_BLACK);                       /* คลื่นเสียง */
    for (int i = 0; i < WAVE_N; i++) wave_h[i] = wave_drawn[i] = 0;
}

void Hud_ScreamReset(void)
{
    Hud_ScreamBlank();
    Hud_ScreamScore(0);
    Hud_ScreamTime(10000);
}

void Hud_ScreamArc(int permille)
{
    if (permille > 1000) permille = 1000;
    int target = permille * ARC_SPAN / 1000;
    if (permille <= 0 || target <= arc_done) return;
    /* วาดเป็นวงกลมเล็กเรียงกันทุก 2 องศา เฉพาะช่วงที่เพิ่มจากครั้งก่อน */
    for (int a = arc_done + 1; a <= target; a++)
        if ((a % 2) == 0 || a == target) arc_dot(a, C_CYAN);
    arc_done = target;
}

void Hud_ScreamScore(int score)
{
    char s[8];
    if (score >= 1000) {
        /* 4 หลักในฟอนต์ 40 กว้างเกินรูกลางวงแหวน พื้นดำของตัวเลขจะกินขอบวงแหวน
           เลยลดเป็นฟอนต์ 32 เฉพาะตอนเต็ม ให้อยู่ในรูพอดี */
        IMG_DrawNumber(&digits40, 84, 108, "   ", C_WHITE, C_BLACK);
        IMG_DrawNumber(&digits32, 120 - 2 * digits32.w, 112, "1000", C_WHITE, C_BLACK);
    } else {
        snprintf(s, sizeof s, "%3d", score < 0 ? 0 : score);
        IMG_DrawNumber(&digits40, 84, 108, s, C_WHITE, C_BLACK);
    }
}

void Hud_ScreamTime(int ms_left)
{
    /* ห้ามใช้ %f: ไลบรารี C ของ CubeIDE ตัดการพิมพ์ทศนิยมทิ้งไว้ตั้งแต่ต้น จะได้ช่องว่าง
       จึงแยกเป็นวินาทีกับเศษสิบเองแบบจำนวนเต็ม */
    if (ms_left < 0)     ms_left = 0;
    if (ms_left > 99900) ms_left = 99900;        /* ช่องกว้าง 4 ตัว สูงสุด 99.9 */
    int t = ms_left / 100;                       /* หน่วยละ 0.1 วินาที */
    char s[12];
    snprintf(s, sizeof s, "%02d.%d", t / 10, t % 10);
    IMG_DrawNumber(&digits32, 148 - 4 * digits32.w, 264, s, C_WHITE, C_BLACK);
}

void Hud_ScreamWave(int pct)
{
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    for (int i = 0; i < WAVE_N - 1; i++) wave_h[i] = wave_h[i + 1];   /* เลื่อนไปทางซ้าย */
    wave_h[WAVE_N - 1] = (uint8_t)(2 + pct * 32 / 100);              /* สูง 2-34 จุด */

    /* วาดเฉพาะแท่งที่ความสูงเปลี่ยน ล้างทั้งกล่องทุกเฟรมแล้วจอจะกะพริบ */
    for (int i = 0; i < WAVE_N; i++) {
        if (wave_h[i] == wave_drawn[i]) continue;
        int x = 21 + i * 6;
        IMG_FillRect(x, 214, 3, 34, C_BLACK);
        IMG_FillRect(x, 231 - wave_h[i] / 2, 3, wave_h[i], C_CYAN);
        wave_drawn[i] = wave_h[i];
    }
}

/* ด่านตบปุ่ม - จอ A กับจอ B ใช้ฟังก์ชันนี้ตัวเดียวกัน หน้าตาจึงเหมือนกันเสมอ
   บอร์ด A: smash_render เรียกตรง ๆ · บอร์ด B: วาดตามข้อความ M<ดวงที่ติด> <คะแนน> <เวลาเหลือ ms> <ดวงที่ตบผิด>
   ปุ่ม i อยู่ที่ x = 54 + (i%3)*47 · y = 73 + (i/3)*47 ขนาด 38x38
   สีแยกตามแถวให้ตรงกับปุ่มจริง  แถวบน (ปุ่ม 1-3) แดง · แถวกลาง (4-6) เหลือง · แถวล่าง (7-9) น้ำเงิน */

#define SM_BAR_ON     0xEA06
#define SM_BAR_OFF    0x38A2
#define SM_LABEL      0xEA06      /* สีป้าย TIME / SCORE ในภาพพื้นหลัง */
#define SM_PANEL      0x1082      /* พื้นกรอบ 3x3 ในภาพพื้นหลัง */
#define SM_RUN_MS     30000

enum { SM_OFF = 0, SM_LIT, SM_WRONG };

typedef struct { uint16_t edge, fill, shine; } sm_colors_t;

/* [แถว][ดับ/ติด]  ดับ = สีเดียวกันแบบมืด คนเล่นจะเห็นตลอดว่าแถวไหนสีอะไร */
static const sm_colors_t SM_ROW[3][2] = {
    { { 0x4041, 0x6082, 0x9146 }, { 0xFC50, 0xEA06, 0xFE17 } },   /* แดง */
    { { 0x41C0, 0x62C0, 0x9406 }, { 0xFF8C, 0xFE80, 0xFFF8 } },   /* เหลือง */
    { { 0x08A8, 0x110C, 0x4252 }, { 0x751F, 0x231F, 0xC69F } },   /* น้ำเงิน */
};
static const sm_colors_t SM_WRONG_COLORS = { 0xC618, 0xFFFF, 0xFFFF };   /* ขาว = ตบผิด */

static int sm_first = 1, sm_look[9], sm_score, sm_tenths, sm_bar;

static void smash_button(int i, int look)
{
    int x = 54 + (i % 3) * 47;
    int y = 73 + (i / 3) * 47;
    const sm_colors_t *c = (look == SM_WRONG) ? &SM_WRONG_COLORS : &SM_ROW[i / 3][look == SM_LIT];
    IMG_FillCircle(x + 19, y + 19, 19, c->edge);
    IMG_FillCircle(x + 19, y + 19, 16, c->fill);
    IMG_FillRect  (x + 10, y + 8, 10, 5, c->shine);
}

/* ป้าย "SCORE" ทับคำว่า "HITS" ในภาพพื้นหลัง (ภาพไม่มีไฟล์ต้นฉบับให้สร้างใหม่)
   ตัวอักษร 5x5 จุดจากฟอนต์ของไลบรารี ขยายจุดละ 3x3 = สูง 15 เท่าป้าย TIME
   แต่ละไบต์ = หนึ่งคอลัมน์ บิต 2 (บนสุด) ถึงบิต 6 (ล่างสุด) */
static void smash_score_label(void)
{
    static const uint8_t glyph[5][5] = {
        { 0x58, 0x54, 0x54, 0x54, 0x34 },   /* S (ตัดมุมให้ไม่เหมือนเลข 5) */
        { 0x7c, 0x44, 0x44, 0x44, 0x44 },   /* C */
        { 0x38, 0x44, 0x44, 0x44, 0x38 },   /* O */
        { 0x7c, 0x14, 0x14, 0x14, 0x68 },   /* R */
        { 0x7c, 0x54, 0x54, 0x54, 0x44 },   /* E */
    };
    IMG_FillRect(20, 273, 42, 18, C_BLACK);             /* ลบ HITS (เส้นใต้ y 295 เก็บไว้) */
    for (int ch = 0; ch < 5; ch++)
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 5; row++)
                if (glyph[ch][col] & (1u << (row + 2)))
                    IMG_FillRect(22 + ch * 17 + col * 3, 275 + row * 3, 3, 3, SM_LABEL);

    IMG_FillRect(140, 268, 78, 30, C_BLACK);            /* ลบ "12/30" ตัวอย่างในภาพ */
}

void Hud_SmashReset(void)
{
    sm_first  = 1;       /* เฟรมถัดไปวาดใหม่ครบ: ปุ่ม 9 ดวง (ภาพพื้นหลังมีดวงตัวอย่างติดอยู่) + ป้าย SCORE */
    sm_score  = -1;
    sm_tenths = -1;
    sm_bar    = -1;
}

void Hud_Smash(int lit_mask, int wrong_mask, int score, int ms_left)
{
    if (ms_left < 0) ms_left = 0;
    if (ms_left > SM_RUN_MS) ms_left = SM_RUN_MS;

    for (int i = 0; i < 9; i++) {          /* วาดเฉพาะปุ่มที่เปลี่ยนสี  บิต i ของ mask = ปุ่ม i */
        int look = (lit_mask & (1 << i)) ? SM_LIT : (wrong_mask & (1 << i)) ? SM_WRONG : SM_OFF;
        if (sm_first || look != sm_look[i]) {
            if (sm_first)                  /* ลบขอบชมพูของดวงตัวอย่างในภาพ ซึ่งกว้างกว่าปุ่มเรา 1 จุด */
                IMG_FillCircle(54 + (i % 3) * 47 + 19, 73 + (i / 3) * 47 + 19, 21, SM_PANEL);
            smash_button(i, look);
            sm_look[i] = look;
        }
    }
    if (sm_first) {
        smash_score_label();
        sm_first = 0;
    }

    if (score != sm_score) {               /* คะแนนสด 0-1000 ชิดขวาที่ x = 216 ตรงที่เดิมของ "12/30" */
        sm_score = score;
        char buf[5];
        snprintf(buf, sizeof buf, "%4d", score);
        IMG_DrawNumber(&digits24, 216 - 4 * digits24.w, 270, buf, C_WHITE, C_BLACK);
    }

    int tenths = ms_left / 100;            /* เวลาเหลือ เช่น "29.9" ชิดขวาที่ x = 204 */
    if (tenths != sm_tenths) {
        sm_tenths = tenths;
        int sec = tenths / 10;
        char buf[5];
        buf[0] = (sec / 10) ? ('0' + sec / 10) : ' ';
        buf[1] = '0' + sec % 10;
        buf[2] = '.';
        buf[3] = '0' + tenths % 10;
        buf[4] = 0;
        IMG_DrawNumber(&digits16, 204 - 4 * digits16.w, 228, buf, C_WHITE, C_BLACK);
    }

    int bar = 196 * ms_left / SM_RUN_MS;   /* แถบเวลา (22,252) กว้าง 196 สูง 6 */
    if (bar != sm_bar) {
        sm_bar = bar;
        if (bar > 0)   IMG_FillRect(22, 252, bar, 6, SM_BAR_ON);
        if (bar < 196) IMG_FillRect(22 + bar, 252, 196 - bar, 6, SM_BAR_OFF);
    }
}
