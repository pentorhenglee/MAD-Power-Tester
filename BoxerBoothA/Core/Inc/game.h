/*
 * game.h - Core ของตู้ คุมลำดับว่าตอนนี้อยู่สถานะไหน
 *          (ตามส่วนที่ 5.2 ของ Boxer-Booth.md)
 */
#ifndef GAME_H
#define GAME_H

void Game_Init(void);      /* เรียกครั้งเดียวก่อนเข้าลูป */
void Game_Tick(void);      /* เรียกทุกลูป */

void Game_OnTap(void);     /* แตะจอ - เรียกจาก EXTI ของ T_IRQ */
void Game_OnPerson(void);  /* PIR เจอคน - เรียกจาก EXTI ของ PIR */

#endif
