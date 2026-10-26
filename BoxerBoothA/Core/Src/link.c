/*
 * link.c - ดูหัวข้อใน link.h   (ไฟล์นี้เหมือนกันทั้งสองบอร์ด)
 *
 * คนทำ: Earth
 *
 *  ส่วนที่เหลือ (ประกอบบรรทัด เก็บคิว กู้ตอน UART error) เขียนไว้ให้แล้ว
 *  เพิ่ม Link_IdleMs() ให้ leds.c รู้ว่าสายเงียบอยู่ จะได้ไม่ปิด interrupt ทับตอนข้อมูลกำลังเข้า
 */
#include "link.h"
#include "main.h"
#include "usart.h"
#include <string.h>

#define LINE_MAX   96      /* ข้อความยาวสุดตอนนี้ราว 30 ตัวอักษร (F) เผื่อไว้ */
#define QUEUE_LEN   8      /* เก็บได้ 7 บรรทัดที่ยังไม่มีใครมาอ่าน (ช่องสุดท้ายไว้แยกคิวเต็ม/ว่าง) */

/* ทำไมต้องมีคิว: บอร์ด A มักส่งสองคำสั่งติดกัน เช่น "P2" แล้ว "Q1" ห่างกันไม่ถึง 1 ms
   แต่ลูปหลักวนทุก 20 ms ถ้าเก็บได้บรรทัดเดียว บรรทัดแรกจะโดนทับหายก่อนมีใครมาอ่าน */
static char    rx_line[LINE_MAX];          /* บรรทัดที่กำลังประกอบ */
static int     rx_len;
static char    queue[QUEUE_LEN][LINE_MAX]; /* บรรทัดที่ประกอบเสร็จแล้ว */
static volatile uint8_t q_head;            /* ช่องที่จะเขียนต่อไป (อินเทอร์รัพต์เขียน) */
static volatile uint8_t q_tail;            /* ช่องที่จะอ่านต่อไป (ลูปหลักอ่าน) */
static uint8_t rx_byte;                    /* ที่พักหนึ่งไบต์ของ HAL */
static volatile uint32_t t_last_rx;        /* เวลาที่รับไบต์ล่าสุด */

/* ============================================================
 *  ช่อง 1 - สั่งรับไบต์ถัดไปแบบอินเทอร์รัพต์
 *  ฟังก์ชันนี้ถูกเรียก 3 ที่: ตอนเริ่ม · หลังรับได้ทุกไบต์ · หลัง UART error
 *  อย่าใช้ HAL_UART_Receive แบบรอ เพราะมันหยุดทั้งเกม
 * ============================================================ */
static void rx_restart(void)
{
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
}

void Link_Init(void)
{
    rx_len = 0;
    q_head = q_tail = 0;
    rx_restart();
}

/* ============================================================
 *  ช่อง 2 - ส่งหนึ่งบรรทัด แล้วตามด้วย '\n'
 *  ส่งแบบรอได้ เพราะข้อความสั้นมาก 20 ตัวอักษร ≈ 1.7 ms ที่ 115200
 * ============================================================ */
void Link_Send(const char *line)
{
    static const uint8_t nl = '\n';
    HAL_UART_Transmit(&huart6, (const uint8_t *)line, (uint16_t)strlen(line), 50);
    HAL_UART_Transmit(&huart6, &nl, 1, 5);
}

uint32_t Link_IdleMs(void)
{
    return HAL_GetTick() - t_last_rx;
}

int Link_Receive(char *buf, int size)
{
    /* กันหูดับ: ถ้าด้วยเหตุใดก็ตาม HAL ไม่ได้รอรับอยู่ (สั่งรับต่อไม่สำเร็จ) ให้สั่งใหม่ตรงนี้ */
    if (huart6.RxState == HAL_UART_STATE_READY) rx_restart();

    if (q_tail == q_head) return 0;               /* คิวว่าง */
    strncpy(buf, queue[q_tail], size - 1);
    buf[size - 1] = 0;
    q_tail = (q_tail + 1) % QUEUE_LEN;
    return 1;
}

/* HAL เรียกฟังก์ชันนี้ทุกครั้งที่รับได้ 1 ไบต์ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART6) return;
    t_last_rx = HAL_GetTick();

    if (rx_byte == '\n') {                        /* จบบรรทัด */
        rx_line[rx_len] = 0;
        uint8_t next = (q_head + 1) % QUEUE_LEN;
        if (rx_len > 0 && next != q_tail) {       /* คิวเต็มก็ทิ้งบรรทัดนี้ ดีกว่าไปทับของเก่า */
            strcpy(queue[q_head], rx_line);
            q_head = next;
        }
        rx_len = 0;
    } else if (rx_byte != '\r' && rx_len < LINE_MAX - 1) {
        rx_line[rx_len++] = (char)rx_byte;
    }

    rx_restart();    /* ถ้าไม่สั่งรับต่อ จะรับได้แค่ตัวอักษรเดียวแล้วเงียบไปเลย */
}

/* สายรบกวนหรืออ่านไม่ทัน (overrun) ทำให้ HAL หยุดรับเอง และไม่มีอะไรเตือน
   อาการคือคุยกันได้สักพักแล้วบอร์ดหนึ่งหูดับ ต้องสั่งรับใหม่ตรงนี้ */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART6) return;
    rx_len = 0;
    rx_restart();
}
