/*
 * adc_dma.h - ADC1 + DMA ที่ด่านกรี๊ดกับด่านต่อยใช้ร่วมกัน (บอร์ด B)
 *
 * ADC1 สแกน 2 ช่องสลับกันไม่หยุด แล้ว DMA เขียนวนลงบัฟเฟอร์เดียว
 *   p[2*i]     = ไมค์   PA3 ADC1_IN3  (rank 1)
 *   p[2*i + 1] = เพียโซ PC0 ADC1_IN10 (rank 2)
 *
 * HAL_ADC_Conv*Callback มีได้ที่เดียวทั้งโปรเจกต์ จึงอยู่ใน adc_dma.c
 * แล้วส่งต่อให้แต่ละด่านผ่าน hook ข้างล่าง (ด่านไหนไม่เขียน ตัว weak ทำงานแทน = ไม่ทำอะไร)
 */
#ifndef ADC_DMA_H
#define ADC_DMA_H

#include <stdint.h>

#define ADC_DMA_LEN     512     /* ครึ่งละ 128 คู่ ≈ 5 ms ต่อครั้ง */

/* เรียกตอน enter() ของด่าน เรียกซ้ำได้ เริ่มจริงแค่ครั้งแรก แล้วปล่อยวิ่งตลอด */
void AdcDma_Start(void);

/* hook: เรียกจาก ISR ทุกครึ่งบัฟเฟอร์ ห้ามทำงานหนัก (หาค่าสูงสุด / สะสมผลรวมพอ) */
void Scream_OnAdcBlock(const uint16_t *p, int pairs);
void Punch_OnAdcBlock(const uint16_t *p, int pairs);

#endif
