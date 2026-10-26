/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  *****************************************************
  *****************************************************
  *****************************************************
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"

#include "ILI9341_Touchscreen.h"
#include <stdio.h>
#include <string.h>

#include "screens.h"
#include "img.h"
#include "digits.h"
#include "hud.h"
#include "sound.h"
#include "slave.h"
#include "leds.h"
#include "stage.h"
#include "adc_dma.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 1 = โหมดเทสต์ไมค์ stage_scream.c เดี่ยวๆ (ออกทั้ง Serial Monitor 115200 + LED บอร์ด + จอ LCD)
   0 = ปิดโหมดเทสต์ (ค่าเริ่มต้นสำหรับรวมโค้ดกับทีม) */
#define TEST_SCREAM_ONLY  0

/* 1 = เดโมไล่ดูหน้าจอเฉย ๆ  ·  0 = เล่นเกมจริงผ่าน state machine
   ตอนนี้เป็น 1 เพราะด่านยังเป็นโครงเปล่า พอเพื่อนเขียนเสร็จให้เปลี่ยนเป็น 0 */
#define DEMO_SCREENS  0

/* 1 = เล่นด่านต่อยวนเองบนบอร์ด B ไม่ต้องมีบอร์ด A (ทับ DEMO_SCREENS)
   จบหมัดโชว์คะแนน 5 วิ แล้วเริ่มใหม่ · ตอนรวมกับบอร์ด A ให้ตั้งเป็น 0 */
#define TEST_PUNCH    0

/* 1 = เทสต์ไฟเส้น WS2812B เดี่ยว ๆ วนโหมดเองทุก 2 วิ (กดปุ่มเขียวข้ามได้)
   จอโชว์เลขโหมด 0-4 ตัวใหญ่ · ตอนรวมกับบอร์ด A ให้ตั้งเป็น 0 */
#define TEST_LEDS     0

/* 1 = เทสต์ DFPlayer เดี่ยว ๆ เล่น 0001-0011.mp3 ทีละไฟล์ จบแล้วไปไฟล์ถัดไปเอง (กดปุ่มเขียวข้ามได้)
   จอโชว์เบอร์ไฟล์ตัวใหญ่ + ขา BUSY ตัวเล็กมุมล่าง (0 = กำลังเล่น 1 = เงียบ) · ตอนรวมให้ตั้งเป็น 0 */
#define TEST_SOUND    0

/* ใช้กับ TEST_SOUND: 1 = เปิดไฟเส้นกะพริบตื้ด ๆ (โหมดบรรยากาศ) ไปพร้อมกับเล่นเพลง
   ถ้าเปิดแล้วเสียงหาย แต่ปิดแล้วเสียงมา = ไฟเส้นทำให้ไฟเลี้ยง/กราวด์ของ DFPlayer กระเพื่อม */
#define TEST_SOUND_WITH_LEDS  0

/* 1 = เล่นเกมจริงตามปกติ แต่พิมพ์ทุกบรรทัดที่รับ/ส่งกับบอร์ด A ออก Tera Term (ST-LINK 115200)
   และพิมพ์ "alive" ทุก 2 วิ ไว้ไล่ว่าคำสั่งหายตรงไหน หรือบอร์ด B ค้าง · ใช้เสร็จตั้งเป็น 0 */
#define DEBUG_LINK    0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
#if TEST_SCREAM_ONLY
extern volatile uint32_t scream_raw_rms;
extern volatile uint32_t scream_raw_mean;
extern int               scream_dbg_lvl;
extern int               scream_dbg_flr;
extern uint32_t          scream_run_ms;
#endif
#if TEST_SCREAM_ONLY || DEBUG_LINK
static void Debug_Init(void);
static void Debug_Print(const char *msg);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if TEST_PUNCH
/* ทำแบบเดียวกับ Slave_Tick ตอนได้คำสั่ง S3 แต่เริ่มเองไม่ต้องรอบอร์ด A
   หน้าสรุปโชว์ค่าดิบ "ยอด/พื้นที่" ตัวเล็กที่มุมล่างซ้าย ไว้คาลิเบรต MAX_AREA */
int      Punch_RawPeak(void);
uint32_t Punch_RawArea(void);

static void test_punch_tick(void)
{
    static int running = 0;
    static uint32_t t_done = 0;

    /* ไฟเส้นทำแทนบอร์ด A ที่ปกติสั่ง L<n>: บรรยากาศตอนบูต · หลอดวัดพลังตอนเล่น
       หมัดโดนแล้วหลอดวิ่งขึ้นตามแรง ค้างไว้ระหว่างหน้าคะแนน 5 วิ แล้วว่างตอนเริ่มรอบใหม่ */
    if (!running) {
        if (t_done == 0) Leds_SetMode(LED_ATTRACT);
        if (HAL_GetTick() - t_done >= 5000) {
            Screen_Draw(stage_punch.screen);
            Leds_SetMode(LED_PLAY);
            stage_punch.enter();
            running = 1;
        }
    } else if (stage_punch.update() == STAGE_DONE) {
        Hud_Result(stage_punch.result, stage_punch.score());
        char raw[24];
        snprintf(raw, sizeof raw, "%d/%lu", Punch_RawPeak(), (unsigned long)Punch_RawArea());
        IMG_DrawNumber(&digits16, 12, 290, raw, 0xF7BE, 0x0000);
        running = 0;
        t_done  = HAL_GetTick();
    } else {
        stage_punch.render();
    }

    Sound_Tick();
    Leds_Tick();
}
#endif

#if TEST_LEDS
/* วนครบ 5 โหมดใน leds.h โหมดละ 4 วิ (กดปุ่มเขียวข้ามได้)
   0 ดับ · 1 บรรยากาศตื้ด ๆ สีรุ้ง · 2 หลอดวัดพลังขึ้นลง เขียว->แดง · 3 รุ้งวิ่ง · 4 แดงหายใจ
   ดูว่าไฟติดครบ 60 ดวง สีตรง ไม่กะพริบ และสว่างพอในห้องจริง */
static void test_leds_tick(void)
{
    static int mode = -1, btn_was = 0;
    static uint32_t t_change = 0;

    int btn  = (HAL_GPIO_ReadPin(BTN_READY_GPIO_Port, BTN_READY_Pin) == GPIO_PIN_RESET);
    int next = (btn && !btn_was) || (HAL_GetTick() - t_change >= 4000);
    btn_was  = btn;

    /* โหมด 2: จำลองหมัดแรงไต่ถึงแดง แล้วหมัดเบาค้างแค่เขียว เหมือนด่านต่อยจริง */
    static int demo_step = 0;
    if (mode == LED_PLAY) {
        uint32_t t = HAL_GetTick() - t_change;
        if (demo_step == 0)              { Leds_SweepTo(100); demo_step = 1; }
        else if (demo_step == 1 && t >= 2000) { Leds_SweepTo(30);  demo_step = 2; }
    } else {
        demo_step = 0;
    }

    if (next) {
        mode = (mode + 1) % 5;
        Leds_SetMode(mode);
        char s[2] = { (char)('0' + mode), 0 };
        IMG_DrawNumber(&digits48, 106, 136, s, 0xF7BE, 0x0000);
        t_change = HAL_GetTick();
    }
    Leds_Tick();
}
#endif

#if TEST_SOUND
/* ฟังว่าเบอร์บนจอตรงกับเสียงที่ออก (ดูรายชื่อใน sound.h) ถ้าไม่ตรง = ชื่อไฟล์ในการ์ดผิด
   ขา BUSY ต้องเป็น 0 ตอนมีเสียง ถ้าค้าง 1 ตลอด = สาย PD7 ไม่ได้ต่อ (ยังเล่นได้ แต่ใช้เวลาเดาแทน) */
static void test_sound_tick(void)
{
    static int track = 0, btn_was = 0;
    static uint32_t t_start = 0;

    int btn  = (HAL_GPIO_ReadPin(BTN_READY_GPIO_Port, BTN_READY_Pin) == GPIO_PIN_RESET);
    int skip = btn && !btn_was;
    btn_was  = btn;

    /* รอ DFPlayer อ่านการ์ด 2 วิแรก แล้วไปไฟล์ถัดไปเมื่อเพลงจบ (เผื่อ 300 ms ให้คำสั่งออกก่อน) */
    if (HAL_GetTick() > 2500 &&
        (track == 0 || skip || (HAL_GetTick() - t_start > 300 && !Sound_IsBusy()))) {
        track = (track % 11) + 1;
        Sound_Play(track);
        char s[3];
        snprintf(s, sizeof s, "%2d", track);
        IMG_DrawNumber(&digits48, 91, 136, s, 0xF7BE, 0x0000);
        t_start = HAL_GetTick();
    }

    char b[2] = { HAL_GPIO_ReadPin(MP3_BUSY_GPIO_Port, MP3_BUSY_Pin) ? '1' : '0', 0 };
    IMG_DrawNumber(&digits16, 12, 290, b, 0xF7BE, 0x0000);
    Sound_Tick();
#if TEST_SOUND_WITH_LEDS
    Leds_Tick();
#endif
}
#endif

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI5_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART6_UART_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  ILI9341_Init();
  ILI9341_Set_Rotation(SCREEN_VERTICAL_2);              /* 240 x 320 แนวตั้ง กลับหัว 180 องศา ตามที่ติดจอบนตู้ · เดิม SCREEN_VERTICAL_1 */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);             /* ไฟหลังจอ */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1000);
  AdcDma_Start();                                       /* ADC1 + DMA วิ่งตลอด ไมค์กับเพียโซใช้ร่วมกัน */

#if TEST_PUNCH
  Leds_Init();
#elif TEST_LEDS
  IMG_FillRect(0, 0, 240, 320, 0x0000);                 /* พื้นดำ ไว้โชว์เลขโหมด */
  Leds_Init();
#elif TEST_SOUND
  IMG_FillRect(0, 0, 240, 320, 0x0000);                 /* พื้นดำ ไว้โชว์เบอร์ไฟล์ */
#if TEST_SOUND_WITH_LEDS
  Leds_Init();
  Leds_SetMode(LED_ATTRACT);                            /* ไฟกะพริบเหมือนตอนเล่นเกมจริง */
#endif
#elif TEST_SCREAM_ONLY
  Debug_Init();
  Leds_Init();
  Screen_Draw(SC_INTRO_SCREAM);
  Debug_Print("\r\n=== Board B: Mic Test (PA3) - Press Blue Button B1 to Start ===\r\n");
#elif DEMO_SCREENS
  Screen_Draw(SC_ATTRACT);
#else
#if DEBUG_LINK
  Debug_Init();
  Debug_Print("\r\n=== Board B: game mode, link debug on ===\r\n");
#endif
  Leds_Init();
  Slave_Init();
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if TEST_PUNCH
	  test_punch_tick();
#elif TEST_LEDS
	  test_leds_tick();
#elif TEST_SOUND
	  test_sound_tick();
#elif TEST_SCREAM_ONLY
	  /* สถานะเทสต์: 0 = รอกด B1, 1 = รัน stage_scream ของจริงจนจบ, 2 = ค้างดูคะแนน 4 วิ */
	  static int state = 0, b1_was = 0;
	  static uint32_t t_mark = 0, t_log = 0;
	  char msg[140];

	  uint32_t now = HAL_GetTick();
	  int b1 = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) ||
	           (HAL_GPIO_ReadPin(BTN_READY_GPIO_Port, BTN_READY_Pin) == GPIO_PIN_RESET) ||
	           TP_Touchpad_Pressed();
	  int pressed = (b1 && !b1_was);
	  b1_was = b1;

	  if (state == 0) {
		  if (now - t_log >= 200) {
			  t_log = now;
			  snprintf(msg, sizeof msg, "[READY - Press B1] RawRMS=%4lu  PA3avg=%4lu (%lu mV)\r\n",
			           scream_raw_rms, scream_raw_mean, scream_raw_mean * 3300UL / 4095UL);
			  Debug_Print(msg);
		  }
		  if (pressed) {
			  Debug_Print("\r\n>>> Starting stage_scream (Prompt -> Quiet 2s -> Scream 10s)...\r\n");
			  stage_scream.enter();
			  state = 1;
		  }
	  } else if (state == 1) {
		  int thr = scream_dbg_flr + 180;
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, (scream_dbg_lvl >= thr) ? GPIO_PIN_SET : GPIO_PIN_RESET); /* LD1 เขียว = ดังถึงเกณฑ์ */
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, (scream_run_ms >= 400)  ? GPIO_PIN_SET : GPIO_PIN_RESET); /* LD2 ฟ้า  = ได้แต้ม */

		  if (stage_scream.update() == STAGE_DONE) {
			  int sc = stage_scream.score();
			  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, GPIO_PIN_RESET);
			  Hud_Result(stage_scream.result, sc);
			  Leds_SetMode(LED_CELEBRATE);
			  snprintf(msg, sizeof msg, "\r\n=== DONE! SCORE = %d / 1000 ===\r\n\r\n", sc);
			  Debug_Print(msg);
			  t_mark = HAL_GetTick();
			  state  = 2;
		  } else {
			  stage_scream.render();
			  if (now - t_log >= 150) {
				  t_log = now;
				  snprintf(msg, sizeof msg, "[SCREAM] RMS=%4lu | Lvl=%d.%d (Flr=%d.%d) | Score=%4d\r\n",
				           scream_raw_rms, scream_dbg_lvl / 10, scream_dbg_lvl % 10,
				           scream_dbg_flr / 10, scream_dbg_flr % 10, stage_scream.score());
				  Debug_Print(msg);
			  }
		  }
	  } else if (state == 2) {
		  if (pressed || (now - t_mark >= 4000)) {
			  Leds_SetMode(LED_OFF);
			  Screen_Draw(SC_INTRO_SCREAM);
			  state = 0;
		  }
	  }
	  Sound_Tick();
	  Leds_Tick();
#elif DEMO_SCREENS
	  /* เดโมไล่ดูหน้าจอ เปลี่ยนเองทุก 3 วิ หรือกดปุ่มเขียว/แตะจอข้าม */
	  static int page = 0;
	  static int btn_was = 0, tp_was = 0;
	  static uint32_t last_change = 0;
	  int next = 0;

	  int btn = (HAL_GPIO_ReadPin(BTN_READY_GPIO_Port, BTN_READY_Pin) == GPIO_PIN_RESET);
	  int tp  = TP_Touchpad_Pressed();
	  if (btn && !btn_was) next = 1;
	  if (tp && !tp_was)   next = 1;
	  btn_was = btn;
	  tp_was  = tp;
	  if (HAL_GetTick() - last_change >= 3000) next = 1;

	  if (next) {
		  page++;
		  if (page >= SC_COUNT) page = 0;
		  Screen_Draw(page);
		  last_change = HAL_GetTick();
	  }
#else
	  Slave_Tick();
#if DEBUG_LINK
	  /* ถ้าบรรทัดนี้หายไป = บอร์ด B ค้าง ไม่ใช่สายหลุด */
	  static uint32_t t_alive = 0;
	  if (HAL_GetTick() - t_alive >= 2000) {
		  t_alive = HAL_GetTick();
		  char msg[64];
		  snprintf(msg, sizeof msg, "[B] alive %lus  mp3_busy_pin=%d\r\n",
		           HAL_GetTick() / 1000,
		           HAL_GPIO_ReadPin(MP3_BUSY_GPIO_Port, MP3_BUSY_Pin) == GPIO_PIN_RESET);
		  Debug_Print(msg);
	  }
#endif
#endif

	  HAL_Delay(20);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
#if TEST_SCREAM_ONLY || DEBUG_LINK
static UART_HandleTypeDef dbg_huart3;

static void Debug_Init(void)
{
  GPIO_InitTypeDef g = {0};
  RCC_PeriphCLKInitTypeDef pclk = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  g.Pin  = GPIO_PIN_13;
  g.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(GPIOC, &g);

  __HAL_RCC_GPIOB_CLK_ENABLE();
  g.Pin  = GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_14;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOB, &g);

  pclk.PeriphClockSelection = RCC_PERIPHCLK_USART3;
  pclk.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
  HAL_RCCEx_PeriphCLKConfig(&pclk);

  __HAL_RCC_USART3_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &g);

  dbg_huart3.Instance        = USART3;
  dbg_huart3.Init.BaudRate   = 115200;
  dbg_huart3.Init.WordLength = UART_WORDLENGTH_8B;
  dbg_huart3.Init.StopBits   = UART_STOPBITS_1;
  dbg_huart3.Init.Parity     = UART_PARITY_NONE;
  dbg_huart3.Init.Mode       = UART_MODE_TX_RX;
  HAL_UART_Init(&dbg_huart3);
}

static void Debug_Print(const char *msg)
{
  HAL_UART_Transmit(&dbg_huart3, (const uint8_t *)msg, (uint16_t)strlen(msg), 50);
}
#endif

#if DEBUG_LINK
/* slave.c เรียกทุกบรรทัดที่รับ/ส่ง (ตัวจริงของ weak ใน slave.c) */
void Dbg_Line(const char *tag, const char *line)
{
  char msg[128];                      /* บรรทัดยาวสุด 95 ตัว (ตารางอันดับ T) + เวลาและป้าย ไม่ให้โดนตัด */
  snprintf(msg, sizeof msg, "%6lu [%s] %s\r\n", HAL_GetTick(), tag, line);
  Debug_Print(msg);
}
#endif
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
