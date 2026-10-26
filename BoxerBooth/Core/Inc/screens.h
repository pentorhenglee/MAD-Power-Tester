/*
 * screens.h - หน้าจอเกม POWER TESTER 20 หน้า ใช้ในเกม 18 หน้า (ILI9341 แนวตั้ง 240x320)
 * ที่มาของดีไซน์: figma.com/make/exOFmDFTnklxUETMLPLFGO ("The Minimal Core")
 * ภาพพรีวิวตรงกับ tools/screens/fm_sheet.png
 *
 * ภาพพื้นหลังทุกหน้าเก็บเป็นข้อมูลบีบอัดแบบ run-length ใน img_*.c (วาดด้วย IMG_Draw)
 * ตัวเลขกับส่วนที่เปลี่ยนระหว่างเล่นวาดทับทีหลังใน hud.c
 */
#ifndef SCREENS_H
#define SCREENS_H

#include <stdint.h>

typedef enum {
    SC_ATTRACT = 0,
    SC_BOARD,
    SC_INTRO_SCREAM,
    SC_READY_3,
    SC_READY_2,
    SC_READY_1,
    SC_QUIET_2,          /* ด่านกรี๊ด: เงียบไว้ ระหว่างวัดเสียงพื้นห้อง */
    SC_QUIET_1,
    SC_SCREAM,
    SC_RESULT_SCREAM,
    SC_INTRO_SMASH,
    SC_SMASH,
    SC_RESULT_SMASH,
    SC_INTRO_PUNCH,
    SC_PUNCH,
    SC_PUNCH_HIT,
    SC_RESULT_PUNCH,
    SC_FINAL,
    SC_FINAL_HIGH,
    SC_ERROR,
    SC_COUNT
} screen_id_t;

/* วาดหน้าเต็มหน้าจอ (ล้างจอให้เอง) */
void Screen_Draw(screen_id_t id);

/* ชื่อหน้าแบบสั้น ไว้ส่งออก UART ตอนดีบัก */
const char *Screen_Name(screen_id_t id);

#endif /* SCREENS_H */
