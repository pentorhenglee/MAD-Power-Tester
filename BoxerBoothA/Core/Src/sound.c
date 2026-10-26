/*
 * sound.c (บอร์ด A) - บอร์ด A ไม่มีลำโพง มันส่งคำสั่งข้ามไปให้บอร์ด B เล่นแทน
 *
 * ใช้ sound.h หน้าตาเดียวกับบอร์ด B ทุกอย่าง game.c จึงเรียกเหมือนกันเป๊ะ
 */
#include "sound.h"
#include "link.h"
#include <stdio.h>

void Sound_Play(int track)
{
    char cmd[8];
    snprintf(cmd, sizeof cmd, "P%d", track);
    Link_Send(cmd);
}

void Sound_Loop(int track)
{
    char cmd[8];
    snprintf(cmd, sizeof cmd, "Q%d", track);
    Link_Send(cmd);
}

void Sound_Stop(void)
{
    Link_Send("X");
}

int Sound_IsBusy(void)
{
    /* บอร์ด A มองไม่เห็นขา BUSY ของ DFPlayer
       ด่านกรี๊ดซึ่งเป็นที่เดียวที่ต้องรอเสียงจบ ทำงานอยู่บอร์ด B อยู่แล้ว
       ฝั่งนี้จึงตอบว่า "ไม่ติดอะไร" ได้ */
    return 0;
}
