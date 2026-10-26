/*
 * stage.h - ข้อตกลงของ "ด่าน" ทุกด่านต้องมี 4 ฟังก์ชันนี้เหมือนกันหมด
 *           (ตามส่วนที่ 5.3 ของ Boxer-Booth.md)   ไฟล์นี้เหมือนกันทั้งสองบอร์ด
 *
 * Core ไม่รู้จักชื่อด่านเลย มันเห็นแค่ตารางของฟังก์ชัน 4 ตัว
 * ผลคือเพิ่มด่านที่ 4 ได้โดยไม่ต้องแก้ Core และแต่ละคนเขียนด่านตัวเองแยกกันได้
 */
#ifndef STAGE_H
#define STAGE_H

#include "screens.h"

typedef enum {
    STAGE_RUNNING = 0,      /* ด่านยังเล่นอยู่ */
    STAGE_DONE              /* ด่านจบแล้ว Core ไปต่อได้ */
} stage_result_t;

typedef enum {
    COUNT_321 = 0,          /* Core นับถอยหลัง 3-2-1 พร้อมเสียง 0004 ให้ก่อนเข้าด่าน */
    COUNT_NONE              /* ด่านจัดช่วงก่อนเริ่มเอง ด่านกรี๊ดต้องเงียบและวัดเสียงพื้นห้อง
                               ซึ่งทำได้แค่บอร์ดที่มีทั้งไมค์ ลำโพง และขา BUSY คือบอร์ด B */
} countdown_t;

typedef struct {
    const char     *name;
    void          (*enter)(void);        /* เรียกครั้งเดียวตอนเข้าด่าน */
    stage_result_t (*update)(void);      /* เรียกทุกลูป ห้ามค้าง ห้าม HAL_Delay */
    int           (*score)(void);        /* คืน 0-1000 เรียกหลัง DONE */
    void          (*render)(void);       /* วาดเฉพาะค่าที่เปลี่ยน ห้ามล้างทั้งจอ */
    screen_id_t     intro;               /* หน้าอธิบายด่าน */
    screen_id_t     screen;              /* หน้าจอตอนเล่น */
    screen_id_t     result;              /* หน้าสรุปคะแนนด่าน */
    countdown_t     countdown;
} stage_t;

/* ด่านทั้งสาม - ประกาศไว้ที่นี่ ตัวจริงอยู่ในไฟล์ของใครของมัน */
extern const stage_t stage_scream;
extern const stage_t stage_smash;
extern const stage_t stage_punch;

#endif
