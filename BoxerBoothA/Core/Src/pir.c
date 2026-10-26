/*
 * pir.c - โมดูลตรวจจับคนเข้า-ออกหน้าตู้ด้วย PIR (บอร์ด A: BoxerBoothA)
 * คนทำ: กัน
 */
#include "pir.h"
#include "game.h"

static uint32_t boot_ms     = 0;
static uint8_t  warmup_done = 0;

volatile uint32_t pir_trig_count = 0;

void PIR_Init(void)
{
    boot_ms        = HAL_GetTick();
    pir_trig_count = 0;
    warmup_done    = (PIR_WARMUP_MS == 0U) ? 1 : 0;
}

int PIR_IsWarmingUp(void)
{
    if (warmup_done) {
        return 0;
    }
    if ((HAL_GetTick() - boot_ms) >= PIR_WARMUP_MS) {
        warmup_done = 1;
        return 0;
    }
    return 1;
}

/* เรียกใน EXTI4_IRQHandler (ขา PA4 ขอบขาขึ้น)
   ถ้ายังไม่พ้น 30 วิแรก จะคืน 0 เพื่อทิ้งสัญญาณรบกวนช่วงวอร์มอัป */
int PIR_AllowIRQ(void)
{
    if (PIR_IsWarmingUp()) {
        return 0;
    }
    pir_trig_count++;
    return 1;
}

/* เรียกทุกรอบก่อน Game_Tick() โดยไม่ต้องแก้ไฟล์ game.c:
   ถ้าตั้งจั๊มเปอร์ H (Retrigger) เวลาคนยืนขยับต่อเนื่อง ขา PA4 จะค้าง HIGH ยาว
   จึงคอยเรียก Game_OnPerson() ต่อเวลาไว้ไม่ให้จอดับครบ 30 วิใส่หน้าคนเล่น */
void PIR_Update(void)
{
    if (PIR_IsWarmingUp()) {
        return;
    }
    if (HAL_GPIO_ReadPin(PIR_OUT_GPIO_Port, PIR_OUT_Pin) == GPIO_PIN_SET) {
        Game_OnPerson();
    }
}
