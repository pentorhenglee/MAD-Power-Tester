/*
 * leds_remote.c (บอร์ด A) - ส่งคำสั่งเปลี่ยนโหมดไฟไปให้บอร์ด B
 */
#include "leds.h"
#include "link.h"
#include <stdio.h>

void Leds_SetMode(int mode)
{
    char cmd[8];
    snprintf(cmd, sizeof cmd, "L%d", mode);
    Link_Send(cmd);
}
