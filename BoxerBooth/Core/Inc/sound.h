/*
 * sound.h - สั่งเล่นเสียงจาก DFPlayer   (หน้าตาเดียวกันทั้งสองบอร์ด)
 * เบอร์ไฟล์ตรงกับ 0001.mp3 … 0011.mp3 ในโฟลเดอร์ mp3 บน microSD
 *
 * บอร์ด B สั่ง DFPlayer เอง · บอร์ด A ไม่มีลำโพง ส่งต่อให้บอร์ด B ผ่านสาย UART
 */
#ifndef SOUND_H
#define SOUND_H

#define SND_ATTRACT_LOOP   1   /* วนตอนรอผู้เล่น 11.3 วิ */
#define SND_PERSON         2   /* PIR เจอคน */
#define SND_START          3   /* กดเริ่มเกม 1.1 วิ */
#define SND_COUNTDOWN      4   /* 3-2-1-Go  ยาว 3.4 วิ */
#define SND_SMASH_BGM      5   /* เพลงด่านตบปุ่ม 31 วิ */
#define SND_PUNCH_WAIT     6   /* ด่านต่อย รอกดพร้อม 74.3 วิ */
#define SND_PUNCH_HIT      7   /* หมัดโดน */
#define SND_SCREAM_PROMPT  8   /* ด่านกรี๊ด เกริ่น 2.3 วิ */
#define SND_STAGE_CLEAR    9   /* ผ่านด่าน 0.6 วิ */
#define SND_GAME_OVER     10   /* จบเกม 8 วิ */
#define SND_NEW_RECORD    11   /* ทำลายสถิติ 3.9 วิ */

/* เล่นไฟล์นี้หนึ่งรอบ ตัดเพลงที่เล่นอยู่ทิ้งทันที และยกเลิกการเล่นวน */
void Sound_Play(int track);

/* เล่นวนไม่หยุด แต่ "รอเพลงที่เล่นอยู่จบก่อน" แล้วค่อยเริ่ม
   ผลคือสั่ง Sound_Play(SND_PERSON) แล้วตามด้วย Sound_Loop(SND_ATTRACT_LOOP) ได้เลย
   เสียงทักจะไม่โดนตัด ใช้กับ 0001 · 0005 · 0006 */
void Sound_Loop(int track);

void Sound_Stop(void);

/* 1 = กำลังเล่นอยู่ หรือเพิ่งสั่งไปแล้วยังไม่ทันเริ่ม */
int  Sound_IsBusy(void);

/* บอร์ด B ต้องเรียกทุกลูป - ปล่อยคำสั่งที่หน่วงไว้ และสั่งเล่นซ้ำเมื่อเพลงวนจบ */
void Sound_Tick(void);

#endif
