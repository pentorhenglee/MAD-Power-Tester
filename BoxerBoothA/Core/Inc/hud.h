/*
 * hud.h - วาดค่าจริงทับภาพพื้นหลัง (ไฟล์นี้เหมือนกันทั้งสองบอร์ด)
 *
 * ภาพทุกหน้าอบมาจากดีไซน์พร้อม "ตัวเลขตัวอย่าง" เช่น 624, 2830, 12/30
 * ถ้าวาดภาพแล้วไม่วาดค่าจริงทับ คนเล่นจะเห็นตัวเลขปลอม
 * ฟังก์ชันในนี้วาดภาพแล้ววาดค่าจริงทับให้ในคำสั่งเดียว พิกัดตรงกับดีไซน์แล้ว
 */
#ifndef HUD_H
#define HUD_H

#include "screens.h"

void Hud_Attract(int best);                       /* หน้า ATTRACT + สถิติสูงสุด (best < 0 = "---") */
void Hud_Result(screen_id_t screen, int score);   /* หน้าสรุปด่าน + คะแนน */
void Hud_Final(int total, const int stage[3], int best, int is_high);

/* ── ด่านกรี๊ด ── */
void Hud_ScreamBlank(void);            /* ลบค่าตัวอย่างทั้งหมด วงแหวนว่าง ไม่มีตัวเลข */
void Hud_ScreamReset(void);            /* เหมือนข้างบน แต่ตั้งคะแนน 0 เวลา 10.0 */
void Hud_ScreamArc(int permille);      /* เติมวงแหวนถึง permille/1000 วาดเฉพาะส่วนที่เพิ่ม */
void Hud_ScreamScore(int score);
void Hud_ScreamTime(int ms_left);
void Hud_ScreamWave(int pct);          /* เลื่อนคลื่นเสียงไปหนึ่งแท่ง แท่งใหม่สูงตามความดัง 0-100 */

/* ── ด่านตบปุ่ม (จอ B วาดตามจอ A ผ่านข้อความ M) ── */
void Hud_SmashReset(void);             /* เรียกหลังวาดภาพ SC_SMASH ครั้งถัดไปวาดใหม่ครบทุกอย่าง */
void Hud_Smash(int lit_mask, int wrong_mask, int score, int ms_left);   /* วาดเฉพาะที่เปลี่ยน: บิต i ของ mask = ปุ่ม i ติด / ตบผิด · คะแนน · เวลา */

#endif
