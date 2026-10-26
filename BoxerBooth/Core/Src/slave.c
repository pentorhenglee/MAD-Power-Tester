/*
 * slave.c - ดูหัวข้อใน slave.h
 *
 * บอร์ด B รอฟังคำสั่งจากบอร์ด A แล้วทำตาม รูปแบบเต็มอยู่ใน link.h
 *   S<n>  เริ่มด่าน n รันจนจบ แล้วตอบ D<n> <คะแนน>
 *   P Q X เสียง      L ไฟ      A W H F  เปลี่ยนหน้าจอ B      Z หรี่จอ B (หลับ)
 */
#include "slave.h"
#include "link.h"
#include "stage.h"
#include "sound.h"
#include "screens.h"
#include "hud.h"
#include "leds.h"
#include "main.h"
#include "tim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const stage_t *active;    /* ด่านที่กำลังรันอยู่ ถ้าว่างคือไม่ได้รัน */
static int active_id;
static int shown_result = -1;    /* หน้าคะแนนที่ด่านของ B เพิ่งวาดเอง กัน H จาก A วาดซ้ำ */
static int shown_score;

/* คะแนน D<n> <s> ส่งซ้ำทุก REPLY_GAP_MS จนกว่าจะได้ยินอะไรจากบอร์ด A (A ส่ง H ทันทีที่ได้คะแนน)
   ถ้า D หายระหว่างทาง A จะไม่ต้องรอ 60 วิจนหมดเวลา  A ทิ้ง D ที่ซ้ำเอง
   */
#define REPLY_GAP_MS   500
#define REPLY_TRIES      6
static char     reply_line[24];
static int      reply_left;      /* ยังต้องส่งซ้ำอีกกี่ครั้ง 0 = A ได้แล้ว */

/* จอ B ตอนนี้เป็นพื้นหลังด่านตบอยู่หรือยัง ถ้า W11 หายแต่ M มา จะวาดพื้นหลังเองก่อน
   (เดิมปุ่มกับคะแนนไปวาดทับหน้า 3-2-1 ที่ค้างอยู่ = พื้นหลังผิดรูป) */
static int      smash_bg;

/* F ล่าสุดที่วาดไปแล้ว บอร์ด A ส่ง F ซ้ำเผื่อหาย ถ้าได้บรรทัดเดิมซ้ำและยังอยู่หน้าสรุป ไม่วาดใหม่ให้จอกะพริบ */
static char     last_f[64];
static uint32_t t_reply;

static const stage_t *stage_by_id(int n)
{
    if (n == 1) return &stage_scream;
    if (n == 3) return &stage_punch;
    return 0;
}

void Slave_Init(void)
{
    active = 0;
    Link_Init();
    Hud_Attract(-1);             /* "---" จนกว่าบอร์ด A จะส่งสถิติจริงมา (A ตอบทันทีที่ได้ R) */
    Link_Send("R");              /* บอกบอร์ด A ว่าพร้อมแล้ว */
}

/* อ่านตัวเลขที่คั่นด้วยเว้นวรรคออกจากบรรทัด คืนจำนวนที่อ่านได้ */
static int parse_ints(const char *s, int *out, int max)
{
    int n = 0;
    char *end;
    while (n < max) {
        long v = strtol(s, &end, 10);
        if (end == s) break;
        out[n++] = (int)v;
        s = end;
    }
    return n;
}

/* ไว้ดีบัก: main.c เขียนตัวจริงเมื่อเปิด DEBUG_LINK ไม่งั้นตัวนี้ทำงานแทน (ไม่ทำอะไร) */
__attribute__((weak)) void Dbg_Line(const char *tag, const char *line) { (void)tag; (void)line; }

/* ไฟหลังจอ B (TIM1 CH1 คาบ 1000) เหมือน backlight() ใน game.c ของบอร์ด A */
static void backlight(int pct)  { __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pct * 10); }

/* บอร์ด A ไปหน้าสรุป / หน้าเรียกแขกแล้ว แต่ด่านของ B ยังค้างอยู่ (จังหวะเหลื่อม / D หาย / A หมดเวลารอ)
   ด่านนั้นไม่มีความหมายแล้ว ปล่อยทิ้ง จอ B จะได้ตามจอ A ไม่ค้างหน้าคะแนน
   */
void Punch_Abort(void);

static void abort_stage(void)
{
    if (!active) return;
    Dbg_Line("ABORT", active->name);
    if (active == &stage_punch) Punch_Abort();   /* ดับไฟปุ่มเขียว */
    active = 0;
}

static void handle(const char *line)
{
    Dbg_Line("RX", line);
    reply_left = 0;                        /* ได้ยินบอร์ด A แล้ว = A ไปต่อแล้ว เลิกส่ง D ซ้ำ */

    /* Z = บอร์ด A หลับ หรี่จอ B ตาม · คำสั่งที่เปลี่ยนหน้าจอ = ตื่นแล้ว เปิดไฟจอเต็ม
       ปกติ A ส่ง A<best> ตอนตื่นอยู่แล้ว แต่รับ S W H F ด้วย กันจอ B มืดค้างถ้า A<best> หาย */
    if (line[0] == 'Z') backlight(5);
    else if (line[0] && strchr("ASWHF", line[0])) backlight(100);
    if (line[0] && strchr("ASWHF", line[0])) smash_bg = 0;   /* คำสั่งเปลี่ยนหน้า: ไม่ใช่พื้นหลังด่านตบแล้ว (W11 ตั้งกลับข้างล่าง) */
    if (line[0] && strchr("ASWH", line[0]))  last_f[0] = 0;  /* ออกจากหน้าสรุปแล้ว F ครั้งหน้าต้องวาด */

    switch (line[0]) {

    case 'S': {                           /* เริ่มด่าน */
        int n = line[1] - '0';
        const stage_t *s = stage_by_id(n);
        if (s) {
            char ack[8];                   /* K<n> = ได้คำสั่งแล้ว ถ้า A ไม่ได้ K จะส่ง S ซ้ำ */
            snprintf(ack, sizeof ack, "K%d", n);
            Link_Send(ack);
            Dbg_Line("TX", ack);
            if (active == s) break;        /* S ซ้ำของด่านที่กำลังเล่นอยู่ (K ก่อนหน้าหาย) ไม่เริ่มใหม่ */
            active    = s;
            active_id = n;
            /* ด่านกรี๊ด (COUNT_NONE) วาดหน้าเกริ่นเองใน enter ไม่ต้องวาดหน้าเล่นให้ก่อน กันจอแวบ */
            if (s->countdown != COUNT_NONE) Screen_Draw(s->screen);
            s->enter();
        }
        break;
    }

    case 'P': Sound_Play(atoi(line + 1));   break;   /* เล่นหนึ่งรอบ */
    case 'Q': Sound_Loop(atoi(line + 1));   break;   /* เล่นวน */
    case 'X': Sound_Stop();                 break;
    case 'L': Leds_SetMode(atoi(line + 1)); break;

    case 'A':                              /* จอ B กลับหน้าเรียกแขก (-1 = ยังไม่มีสถิติ ขึ้น "---") */
        abort_stage();                     /* A กลับหน้าเรียกแขกแล้ว ด่านไหนค้างก็จบ */
        Hud_Attract(atoi(line + 1));
        break;

    /* จอ B วาดตามจอ A ตอนที่ไม่มีด่านของบอร์ด B รันอยู่ (ด่านของ B วาดจอเอง) */
    case 'W': {                            /* ขึ้นหน้าเดียวกับจอ A เช่น หน้าอธิบายด่าน 3-2-1 */
        int id = atoi(line + 1);
        if (!active && id >= 0 && id < SC_COUNT) {
            Screen_Draw((screen_id_t)id);
            if (id == SC_SMASH) { Hud_SmashReset(); smash_bg = 1; }
        }
        break;
    }

    case 'H': {                            /* หน้าคะแนนด่าน + คะแนนจริง */
        int v[2] = {0};
        if (!active && parse_ints(line + 1, v, 2) == 2 && v[0] >= 0 && v[0] < SC_COUNT
            && !(v[0] == shown_result && v[1] == shown_score))   /* ด่านของ B วาดหน้านี้ไปแล้ว ไม่วาดซ้ำให้จอกะพริบ */
            Hud_Result((screen_id_t)v[0], v[1]);
        shown_result = -1;
        break;
    }

    case 'F': {                            /* จอ B ขึ้นหน้าสรุปรวม */
        if (strcmp(line, last_f) == 0) break;   /* F ซ้ำที่ A ส่งมากันหาย วาดไปแล้ว */
        strncpy(last_f, line, sizeof last_f - 1);
        last_f[sizeof last_f - 1] = 0;
        int v[6] = {0};
        if (parse_ints(line + 1, v, 6) == 6) {
            abort_stage();                 /* A ขึ้นหน้าสรุปแล้ว จอ B ต้องขึ้นตามเสมอ (เดิมทิ้ง F ถ้าด่านยังค้าง = จอค้างหน้าคะแนน) */
            int stage[3] = { v[1], v[2], v[3] };
            Hud_Final(v[0], stage, v[4], v[5]);
        }
        break;
    }
    }
}

void Slave_Tick(void)
{
    char line[96];                        /* เท่า LINE_MAX ใน link.c ข้อความตารางอันดับยาวราว 75 ตัว */

    /* ── รับคำสั่ง อ่านจนคิวหมดในรอบเดียว ──
       บอร์ด A ส่งทีละ 3-4 บรรทัดติดกัน (เช่น A2830 L1 Q1) ถ้าอ่านรอบละบรรทัด
       แล้วรอบไหนวาดจอทั้งหน้านาน คิวจะเต็มจนคำสั่งหาย เพลงไม่ขึ้น */
    /* M = ด่านตบปุ่มสดจากบอร์ด A มาทุก 0.1 วิ วาดแค่ค่าล่าสุด ข้ามของเก่า จอจะไม่ช้ากว่าจอ A */
    char last_m[96];
    int  have_m = 0;
    while (Link_Receive(line, sizeof line)) {
        if (line[0] == 'M') { strcpy(last_m, line); have_m = 1; }
        else handle(line);
    }
    if (have_m && !active) {
        if (!smash_bg) {                   /* W11 หายระหว่างทาง วาดพื้นหลังด่านตบเองก่อนวาดปุ่ม */
            Screen_Draw(SC_SMASH);
            Hud_SmashReset();
            smash_bg = 1;
        }
        int v[4] = { 0, 0, 0, 0 };        /* mask ดวงที่ติด · คะแนน · เวลาเหลือ · mask ดวงที่ตบผิด */
        if (parse_ints(last_m + 1, v, 4) >= 3) {
            Hud_Smash(v[0], v[3], v[1], v[2]);
            Leds_SetLevel(v[1] / 10);      /* หลอดไฟวัดพลังขึ้นตามคะแนนสดของด่านตบปุ่ม */
        }
    }

    /* ── ถ้ากำลังรันด่านอยู่ ก็เดินด่านต่อ ── */
    if (active) {
        if (active->update() == STAGE_DONE) {
            int s = active->score();
            char reply[24];
            snprintf(reply, sizeof reply, "D%d %d", active_id, s);
            Link_Send(reply);
            Dbg_Line("TX", reply);
            strcpy(reply_line, reply);     /* เตรียมส่งซ้ำถ้า A ยังเงียบ */
            reply_left = REPLY_TRIES;
            t_reply    = HAL_GetTick();
            Hud_Result(active->result, s);
            shown_result = active->result;
            shown_score  = s;      /* คะแนนจริงบนจอใหญ่ ไม่ใช่เลขตัวอย่างในภาพ */
            active = 0;
        } else {
            active->render();
        }
    }

    if (reply_left && HAL_GetTick() - t_reply >= REPLY_GAP_MS) {
        Link_Send(reply_line);
        Dbg_Line("TX", reply_line);
        reply_left--;
        t_reply = HAL_GetTick();
    }

    Sound_Tick();
    Leds_Tick();
}
