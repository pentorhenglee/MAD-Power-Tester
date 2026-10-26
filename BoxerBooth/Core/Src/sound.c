/*
 * sound.c (บอร์ด B) - สั่ง DFPlayer เล่นไฟล์ ดูหัวข้อใน sound.h
 *
 *
 * DFPlayer รับคำสั่งเป็นชุด 10 ไบต์ผ่าน UART 9600 8N1
 *
 *   7E FF 06 CMD 00 PH PL CKH CKL EF
 *   │  │  │  │   │  └┬─┘ └─┬─┘  └─ จบเฟรม
 *   │  │  │  │   │   │     └─ เช็กซัม
 *   │  │  │  │   │   └─ พารามิเตอร์ 2 ไบต์ (เบอร์ไฟล์)
 *   │  │  │  │   └─ 00 = ไม่ต้องตอบกลับ, 01 = ให้ตอบ
 *   │  │  │  └─ คำสั่ง  0x12 = เล่นไฟล์ในโฟลเดอร์ mp3 ตามชื่อ, 0x16 = หยุด
 *   │  │  └─ ความยาวตั้งแต่ไบต์นี้ถึงก่อนเช็กซัม = 6 เสมอ
 *   │  └─ เวอร์ชัน
 *   └─ ไบต์เริ่มเฟรม
 *
 *   เช็กซัม = 0 ลบด้วยผลบวกของไบต์ที่ 2 ถึง 7  (ไม่รวม 7E และ EF)
 *
 * ส่วนที่เขียนไว้ให้แล้ว (ไม่ต้องแตะ)
 *   - คิวเว้นจังหวะ: DFPlayer ทำคำสั่งตกถ้าส่งติดกันเร็วกว่า 100 ms
 *   - เล่นวน: DFPlayer เล่นจบแล้วเงียบ ไม่วนให้เอง
 *   - รู้ว่าเพลงจบหรือยัง: ดูขา MP3_BUSY และกันกับดักที่ขานี้ไม่ลงต่ำทันทีหลังสั่งเล่น
 */
#include "sound.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>

/* 0x12 เล่น mp3/0001.mp3 ตามชื่อไฟล์จริง · 0x03 เล่นตามลำดับที่ก๊อปลงการ์ด ซึ่งไม่ตรงชื่อ */
#define CMD_PLAY      0x12
#define CMD_STOP      0x16
#define CMD_GAP_MS     120     /* เว้นระหว่างคำสั่งอย่างน้อยเท่านี้ */
#define START_MS      1000     /* หลังสั่งเล่น รอขา BUSY ลงต่ำได้นานสุดเท่านี้ */
#define BOOT_MS       2000     /* DFPlayer ต้องใช้เวลาอ่านการ์ดหลังเปิดไฟ ส่งก่อนนี้คำสั่งหาย */

/* ความยาวไฟล์ (ms) ใช้เป็นแผนสำรองเมื่อขา BUSY ยังไม่เคยลงต่ำเลย
   เช่นยังไม่ได้ต่อสาย ไฟล์ที่ไม่รู้ความยาวเดาไว้ ต้องวัดจริง */
static const uint32_t track_ms[12] = {
    0, 11300, 2500, 1100, 3400, 31000, 74300, 1000, 2300, 600, 8000, 3900
};

static uint32_t t_last_cmd;
static int      pending_cmd;       /* 0 = ไม่มีอะไรค้างในคิว */
static int      pending_param;
static uint32_t t_play;            /* เวลาที่ส่งคำสั่งเล่นล่าสุด 0 = ไม่ได้เล่น */
static int      playing;           /* ไฟล์ที่สั่งเล่นล่าสุด */
static int      seen_low;          /* หลังสั่งเล่นล่าสุด เห็น BUSY ลงต่ำแล้วหรือยัง */
static int      busy_works;        /* เคยเห็นขา BUSY ทำงานสักครั้งแล้วหรือยัง */
static int      loop_track;        /* เพลงที่ต้องวน 0 = ไม่วน */

static void send_now(int cmd, int param);

/* ส่งจริง แล้วจดว่าส่งอะไรไปเมื่อไหร่ */
/* ดีบัก: main.c โหมด DEBUG_LINK พิมพ์ทุกคำสั่งที่ส่งถึง DFPlayer จริง (ตัว weak อยู่ใน slave.c) */
void Dbg_Line(const char *tag, const char *line);

static void transmit(int cmd, int param)
{
    char s[16];
    snprintf(s, sizeof s, "%s %d", cmd == CMD_PLAY ? "play" : "stop", param);
    Dbg_Line("MP3", s);
    send_now(cmd, param);
    t_last_cmd = HAL_GetTick();
    if (cmd == CMD_PLAY) { t_play = t_last_cmd; playing = param; seen_low = 0; }
    if (cmd == CMD_STOP) { t_play = 0; }
}

/* ส่งเลยถ้าห่างจากคำสั่งก่อนพอแล้ว ไม่งั้นพักไว้ ให้ Sound_Tick ปล่อยทีหลัง */
static void queue(int cmd, int param)
{
    if (HAL_GetTick() >= BOOT_MS && HAL_GetTick() - t_last_cmd >= CMD_GAP_MS) {
        transmit(cmd, param);
        pending_cmd = 0;
    } else {
        pending_cmd   = cmd;        /* คำสั่งใหม่ทับคำสั่งเก่าที่ยังไม่ได้ส่ง */
        pending_param = param;
    }
}

static int busy_pin(void)
{
#ifdef MP3_BUSY_Pin
    return HAL_GPIO_ReadPin(MP3_BUSY_GPIO_Port, MP3_BUSY_Pin) == GPIO_PIN_RESET;  /* LOW = เล่นอยู่ */
#else
    return 0;
#endif
}

int Sound_IsBusy(void)
{
    if (pending_cmd == CMD_PLAY) return 1;          /* สั่งแล้ว ยังรอคิวอยู่ */
    if (t_play == 0) return 0;                       /* ไม่ได้เล่นอะไร */

    if (busy_pin()) { seen_low = 1; busy_works = 1; return 1; }
    if (seen_low) return 0;                          /* เคยลงต่ำแล้วกลับขึ้นสูง = จบแล้ว */

    /* ยังไม่เห็นลงต่ำ: อาจเพราะเพิ่งสั่ง ขายังไม่ทันลง (ช่วงนี้ราว 100-300 ms) */
    uint32_t age = HAL_GetTick() - t_play;
    if (age < START_MS) return 1;
    /* เกิน START_MS แล้วขายังไม่ลง: ขา BUSY ไม่ทำงาน (ไม่ได้ต่อ หรือสายหลวมกลางเกม) เดาจากความยาวไฟล์
       เดิมถ้าขาเคยทำงานแล้วจะตอบว่า "จบแล้ว" ทันที เพลงวนเลยถูกสั่งเริ่มใหม่ทุก 1 วิ เพลงสะดุด/เงียบ
       */
    uint32_t len = (playing > 0 && playing < 12) ? track_ms[playing] : START_MS;
    return age < len;
}

void Sound_Play(int track)
{
    loop_track = 0;
    queue(CMD_PLAY, track);
}

void Sound_Loop(int track)
{
    loop_track = track;            /* Sound_Tick จะเริ่มให้เองเมื่อเพลงที่เล่นอยู่จบ */
}

void Sound_Stop(void)
{
    loop_track = 0;
    queue(CMD_STOP, 0);
}

void Sound_Tick(void)
{
    if (pending_cmd && HAL_GetTick() >= BOOT_MS && HAL_GetTick() - t_last_cmd >= CMD_GAP_MS) {
        int c = pending_cmd;
        pending_cmd = 0;
        transmit(c, pending_param);
    }
    if (loop_track && !Sound_IsBusy())
        queue(CMD_PLAY, loop_track);
}

/* ประกอบเฟรม 10 ไบต์แล้วส่งออก USART2 ไปที่ DFPlayer */
static void send_now(int cmd, int param)
{
    uint8_t f[10] = { 0x7E, 0xFF, 0x06, (uint8_t)cmd, 0x00,
                      (uint8_t)(param >> 8), (uint8_t)param,   /* PH PL */
                      0, 0,                                    /* เช็กซัม เติมข้างล่าง */
                      0xEF };
    uint16_t sum = 0;
    for (int i = 1; i <= 6; i++) sum += f[i];                  /* ไบต์ที่ 2 ถึง 7 */
    uint16_t ck = (uint16_t)(0 - sum);
    f[7] = (uint8_t)(ck >> 8);
    f[8] = (uint8_t)ck;
    HAL_UART_Transmit(&huart2, f, sizeof f, 20);
}
