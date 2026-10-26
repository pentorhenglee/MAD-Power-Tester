/*
 * slave.h - บอร์ด B ทำตามคำสั่งที่บอร์ด A ส่งมา
 *           บอร์ด B ไม่มี state machine ของตัวเอง มันเป็นลูกมือ
 */
#ifndef SLAVE_H
#define SLAVE_H

void Slave_Init(void);
void Slave_Tick(void);

#endif
