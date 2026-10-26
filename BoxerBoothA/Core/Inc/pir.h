/*
 * pir.h - โมดูลตรวจจับคนเข้า-ออกหน้าตู้ด้วย PIR (บอร์ด A: ขา PA4 / EXTI4)
 */
#ifndef PIR_H
#define PIR_H

#include "main.h"
#include <stdint.h>

/* เวลาวอร์มอัปหลังจ่ายไฟ 30 วินาที (ตอนเทสต์หน้าโต๊ะลดชั่วคราวเป็น 3000U ได้) */
#define PIR_WARMUP_MS   30000U

extern volatile uint32_t pir_trig_count;   /* จำนวนครั้งที่เกิด EXTI4 (ไว้ดูตอนดีบัก) */

void PIR_Init(void);          /* เรียกครั้งเดียวตอนเปิดบอร์ด เริ่มนับเวลาวอร์มอัป */
int  PIR_IsWarmingUp(void);   /* คืน 1 ถ้ายังอยู่ในช่วงวอร์มอัป 30 วิแรก */
int  PIR_AllowIRQ(void);      /* เรียกใน EXTI4_IRQHandler: คืน 0 ถ้ายังวอร์มอัปอยู่ */
void PIR_Update(void);        /* เรียกใน while(1) ก่อน Game_Tick() ดูแลโหมด H retrigger */

#endif /* PIR_H */
