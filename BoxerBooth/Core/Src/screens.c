/* screens.c - หน้าจอเกม POWER TESTER ดูหัวข้อใน screens.h
 *
 * ภาพนิ่งของทุกหน้าถูกอบมาจากดีไซน์ Figma แล้วเก็บแบบ RLE ใน img_*.c
 * (สร้างด้วย tools/screens/png2rle.py จาก tools/screens/fm_*.png)
 * ตรงนี้จึงเหลือแค่เลือกภาพให้ตรงกับหน้าที่ต้องการ
 *
 * ค่าที่เปลี่ยนระหว่างเล่น เช่น คะแนนหรือเวลา จะวาดทับลงไปทีหลัง
 */
#include "screens.h"
#include "screens_img.h"
#include "img.h"

static const uint8_t *const screen_img[SC_COUNT] = {
    img_attract,                /* SC_ATTRACT       */
    img_board,                  /* SC_BOARD         */
    img_intro_scream,           /* SC_INTRO_SCREAM  */
    img_ready_3,                /* SC_READY_3       */
    img_ready_2,                /* SC_READY_2       */
    img_ready_1,                /* SC_READY_1       */
    img_quiet_2,                /* SC_QUIET_2       */
    img_quiet_1,                /* SC_QUIET_1       */
    img_scream,                 /* SC_SCREAM        */
    img_result_scream,          /* SC_RESULT_SCREAM */
    img_intro_smash,            /* SC_INTRO_SMASH   */
    img_smash,                  /* SC_SMASH         */
    img_result_smash,           /* SC_RESULT_SMASH  */
    img_intro_punch,            /* SC_INTRO_PUNCH   */
    img_punch,                  /* SC_PUNCH         */
    img_punch_hit,              /* SC_PUNCH_HIT     */
    img_result_punch,           /* SC_RESULT_PUNCH  */
    img_final,                  /* SC_FINAL         */
    img_final_high,             /* SC_FINAL_HIGH    */
    img_error,                  /* SC_ERROR         */
};

static const char *const screen_name[SC_COUNT] = {
    "ATTRACT", "ALL TIME BEST", "INTRO SCREAM", "GET READY 3", "GET READY 2",
    "GET READY 1", "STAY QUIET 2", "STAY QUIET 1", "SCREAM", "RESULT SCREAM",
    "INTRO SMASH", "SMASH", "RESULT SMASH", "INTRO PUNCH", "PUNCH",
    "PUNCH HIT NOW", "RESULT PUNCH", "FINAL SCORE", "FINAL NEW HIGH", "OUT OF ORDER"
};

void Screen_Draw(screen_id_t id)
{
    if (id < SC_COUNT)
        IMG_Draw(screen_img[id]);
}

const char *Screen_Name(screen_id_t id)
{
    return (id < SC_COUNT) ? screen_name[id] : "?";
}
