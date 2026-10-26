/*
 * adc_dma.c - ดูหัวข้อใน adc_dma.h
 */
#include "adc_dma.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

/* aligned 32 = ตรงกับบรรทัดแคช invalidate แล้วไม่ไปโดนตัวแปรข้างเคียง */
static uint16_t buf[ADC_DMA_LEN] __attribute__((aligned(32)));
static int started;

__attribute__((weak)) void Scream_OnAdcBlock(const uint16_t *p, int pairs) { (void)p; (void)pairs; }
__attribute__((weak)) void Punch_OnAdcBlock(const uint16_t *p, int pairs)  { (void)p; (void)pairs; }

void AdcDma_Start(void)
{
    if (started) return;
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)buf, ADC_DMA_LEN) == HAL_OK) started = 1;
}

static void block(uint16_t *p)
{
    /* D-Cache เปิดอยู่ ต้องทิ้งของเก่าในแคชก่อนอ่าน ไม่งั้นได้ค่าเดิมค้าง */
    SCB_InvalidateDCache_by_Addr((void *)p, (ADC_DMA_LEN / 2) * sizeof(uint16_t));
    Scream_OnAdcBlock(p, ADC_DMA_LEN / 4);
    Punch_OnAdcBlock(p, ADC_DMA_LEN / 4);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) block(&buf[0]);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) block(&buf[ADC_DMA_LEN / 2]);
}
