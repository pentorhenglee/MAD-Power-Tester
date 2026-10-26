/*
 * stage_remote.c - ด่านกรี๊ดกับด่านต่อย มองจากฝั่งบอร์ด A
 *
 * เซนเซอร์ของสองด่านนี้อยู่บอร์ด B ไฟล์นี้จึงไม่ได้ทำงานเอง
 * มันแค่สั่งบอร์ด B ให้ลงมือ แล้วรอคะแนนกลับมา
 *
 * ด่านกรี๊ดเล่นในห้องมืดหลังม่าน คนเล่นเห็นแค่จอบอร์ด A บนกล่องปุ่ม
 * บอร์ด B จึงส่งทุกจังหวะมาให้วาด (V0 เกริ่น · V1 V2 เงียบ · V3 คะแนนสดทุก 0.1 วิ)
 * ด่านต่อยอยู่ข้างนอก จอบอร์ด B ที่ผนังขวาแสดงเอง จอ A ไม่ต้องทำอะไร
 *
 * จุดสำคัญ: มันใช้ "สัญญาของด่าน" ชุดเดียวกับด่านที่ทำงานเอง
 *           game.c จึงไม่รู้เลยว่าด่านนี้อยู่คนละบอร์ด - ไม่ต้องแก้ Core สักบรรทัด
 *
 */
#include "stage.h"
#include "link.h"
#include "hud.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ด่านกรี๊ดบนบอร์ด B ใช้ราว 15 วิ ด่านต่อยรอปุ่มเขียวได้ 30 วิ บวกต่อยอีก 15
   เลยเผื่อไว้ 60 วิ เกินนี้ถือว่าบอร์ด B ไม่ตอบ */
/* ถ้าจะเทสด่านตบปุ่มเดี่ยวโดยไม่ต่อบอร์ด B ลดเหลือ 3000 ชั่วคราวได้ แต่ต่อจริงต้อง 60000 */
#define REMOTE_TIMEOUT  60000


static int      pending_stage;   /* ด่านที่สั่งไปแล้วกำลังรอคำตอบ */
static int      got_score;
static int      done;
static uint32_t t_sent;

/* S<n> ต้องได้ K<n> ตอบกลับ ถ้าเงียบเกิน ACK_MS ส่ง S ซ้ำ ครบ ACK_TRIES แล้วยังเงียบ = B ไม่ตอบ
   ไปต่อเลยภายใน ~3 วิ ไม่ต้องรอ REMOTE_TIMEOUT 60 วิ (เดิม S หายครั้งเดียว จอค้างที่ด่านเกือบนาที)
   */
#define ACK_MS      800
#define ACK_TRIES     3
static int      got_ack;
static int      ack_tries;
static uint32_t t_ack;
static int      scr_phase;       /* จังหวะของด่านกรี๊ดที่วาดอยู่บนจอ A ตอนนี้ */
static int      scr_score;       /* คะแนนที่วาดอยู่ วาดใหม่เฉพาะตอนเปลี่ยน จอจะได้ไม่กะพริบ */

/* V<จังหวะ> <คะแนน> <เวลาเหลือ> <ความดัง> จากบอร์ด B ระหว่างด่านกรี๊ด */
static void on_scream(const char *line)
{
    int ph = line[1] - '0';
    int v[3] = { 0, 0, 0 };
    sscanf(line + 2, "%d %d %d", &v[0], &v[1], &v[2]);   /* คะแนน เวลาเหลือ ความดัง */
    if (ph != scr_phase) {
        scr_phase = ph;
        if      (ph == 0) Screen_Draw(SC_INTRO_SCREAM);
        else if (ph == 1) Screen_Draw(SC_QUIET_2);
        else if (ph == 2) Screen_Draw(SC_QUIET_1);
        else if (ph == 3) { Screen_Draw(SC_SCREAM); Hud_ScreamReset(); scr_score = 0; }
    }
    if (ph == 3) {
        if (v[0] != scr_score) {
            scr_score = v[0];
            Hud_ScreamScore(v[0]);
            Hud_ScreamArc(v[0]);
        }
        Hud_ScreamTime(v[1]);
        Hud_ScreamWave(v[2]);
    }
}

static void remote_start(int n)
{
    char cmd[8];
    snprintf(cmd, sizeof cmd, "S%d", n);
    Link_Send(cmd);
    pending_stage = n;
    got_score = 0;
    done = 0;
    t_sent = HAL_GetTick();
    got_ack   = 0;
    ack_tries = 0;
    t_ack     = t_sent;
}

static stage_result_t remote_update(void)
{
    char line[64];
    char last_v[64];
    int  have_v = 0;

    /* อ่านจนคิวหมดในรอบเดียว แล้ววาดเฉพาะ V ล่าสุด
       บอร์ด B ส่ง V3 ทุก 0.1 วิ ถ้าอ่านรอบละบรรทัดแล้วรอบไหนวาดวงแหวนนาน
       บรรทัดจะค้างคิวสะสม จอ A ยิ่งเล่นยิ่งช้ากว่าเสียงจริง - ข้ามค่าเก่าไปวาดค่าสดเลย */
    while (Link_Receive(line, sizeof line)) {
        /* รูปแบบที่รอคือ  D<n> <score>  บรรทัดอื่นทิ้งไป */
        int for_me = (line[1] - '0') == pending_stage;
        if (line[0] == 'D' && for_me) {
            got_score = atoi(line + 2);
            done = 1;
            got_ack = 1;
        } else if (line[0] == 'K' && for_me) {
            got_ack = 1;                   /* B ได้คำสั่งเริ่มด่านแล้ว */
        } else if (line[0] == 'V' && pending_stage == 1) {
            strcpy(last_v, line);
            have_v = 1;
            got_ack = 1;                   /* V มาแปลว่าด่านกรี๊ดเริ่มแล้ว */
        }
    }
    if (have_v && !done) on_scream(last_v);

    if (done) return STAGE_DONE;

    if (!got_ack && HAL_GetTick() - t_ack >= ACK_MS) {
        if (ack_tries >= ACK_TRIES) {
            got_score = 0;                 /* B ไม่ตอบเลย ให้ศูนย์แล้วไปต่อ */
            return STAGE_DONE;
        }
        char cmd[8];                       /* S หายระหว่างทาง ส่งซ้ำ */
        snprintf(cmd, sizeof cmd, "S%d", pending_stage);
        Link_Send(cmd);
        ack_tries++;
        t_ack = HAL_GetTick();
    }

    if (HAL_GetTick() - t_sent >= REMOTE_TIMEOUT) {
        got_score = 0;           /* บอร์ด B เงียบ ให้ศูนย์แล้วไปต่อ ดีกว่าค้างทั้งเกม */
        return STAGE_DONE;
    }
    return STAGE_RUNNING;
}

static int  remote_score(void) { return got_score; }
static void remote_render(void) { /* จอบอร์ด A ไม่ต้องวาดอะไรระหว่างนี้ */ }

static void scream_enter(void)
{
    /* ไม่วาดหน้าอธิบายซ้ำ: game.c ขึ้นหน้านี้ไว้แล้วตอนเข้าหน้าอธิบาย และไม่วาดหน้าเล่นทับให้ด่านนี้
       (วาดซ้ำทำให้จอไล่ใหม่จากบนลงล่างตอนแตะเริ่ม และชื่อ "YOU ARE ..." หายกลางทาง) */
    scr_phase = 0;
    remote_start(1);
}

static void punch_enter(void)  { remote_start(3); }

const stage_t stage_scream = {
    .name      = "SCREAM",
    .enter     = scream_enter,
    .update    = remote_update,
    .score     = remote_score,
    .render    = remote_render,
    .intro     = SC_INTRO_SCREAM,
    .screen    = SC_SCREAM,
    .result    = SC_RESULT_SCREAM,
    .countdown = COUNT_NONE,     /* บอร์ด B จัดคำเกริ่นกับช่วงเงียบเอง */
};

const stage_t stage_punch = {
    .name      = "PUNCH",
    .enter     = punch_enter,
    .update    = remote_update,
    .score     = remote_score,
    .render    = remote_render,
    .intro     = SC_INTRO_PUNCH,
    .screen    = SC_PUNCH,
    .result    = SC_RESULT_PUNCH,
    .countdown = COUNT_321,
};
