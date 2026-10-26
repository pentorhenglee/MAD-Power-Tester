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
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
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
#include "game.h"
#include "link.h"
#include "pir.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 1 = โหมดเทสต์ PIR เดี่ยวๆ (ดูผลผ่าน Serial Monitor 115200 + ไฟ LED บนบอร์ด + จอ LCD)
   0 = ปิดโหมดเทสต์ (ค่าเริ่มต้นสำหรับรวมโค้ดกับทีม) */
#define TEST_PIR_ONLY  0

/* 1 = เดโมไล่ดูหน้าจอเฉย ๆ  ·  0 = เล่นเกมจริงผ่าน state machine
   ตอนนี้เป็น 1 เพราะด่านยังเป็นโครงเปล่า พอเพื่อนเขียนเสร็จให้เปลี่ยนเป็น 0 */
#define DEMO_SCREENS  0

/* 1 = เล่นเกมจริง (ต่อบอร์ด B ได้ปกติ) แต่ให้ไฟ 3 ดวงบนบอร์ดบอกสถานะ PIR ไว้ดูตอนเทสตู้
       LD1 เขียว = ขา PA4 สด · LD2 ฟ้า = จอตื่น (ไม่ใช่ IDLE) · LD3 แดงกะพริบ = PIR ยังวอร์มอัป
   0 = ปิด (ค่าเริ่มต้นสำหรับรวมโค้ดกับทีม) */
#define DEBUG_PIR_LEDS  1

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
#if TEST_PIR_ONLY
static void Debug_Init(void);
static void Debug_Print(const char *msg);
#endif
#if DEBUG_PIR_LEDS
static void PirLeds_Init(void);
static void PirLeds_Update(void);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_USART6_UART_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  ILI9341_Init();
  ILI9341_Set_Rotation(SCREEN_VERTICAL_1);              /* 240 x 320 แนวตั้ง */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);             /* ไฟหลังจอ */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1000);
  PIR_Init();

#if TEST_PIR_ONLY
  Debug_Init();
  Game_Init();
  Debug_Print("\r\n=== Board A: PIR Test (PA4 / EXTI4) ===\r\n");
#elif DEMO_SCREENS
  Screen_Draw(SC_ATTRACT);
#else
  Link_Init();
  Game_Init();
#if DEBUG_PIR_LEDS
  PirLeds_Init();
#endif
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if TEST_PIR_ONLY
	  PIR_Update();
	  Game_Tick();

	  int pg4        = (HAL_GPIO_ReadPin(PIR_OUT_GPIO_Port, PIR_OUT_Pin) == GPIO_PIN_SET);
	  int is_attract = (__HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1) > 100);

	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  pg4 ? GPIO_PIN_SET : GPIO_PIN_RESET);        /* LD1 เขียว = ขา PA4 สด */
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,  is_attract ? GPIO_PIN_SET : GPIO_PIN_RESET); /* LD2 ฟ้า  = หน้า ATTRACT */
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14,
	      (PIR_IsWarmingUp() && (HAL_GetTick() / 300) % 2) ? GPIO_PIN_SET : GPIO_PIN_RESET); /* LD3 แดง = วอร์มอัป */

	  static uint32_t t_log = 0;
	  if (HAL_GetTick() - t_log >= 250) {
		  t_log = HAL_GetTick();
		  char msg[120];
		  snprintf(msg, sizeof msg, "[%s] PA4=%d | EXTI4_Cnt=%lu\r\n",
		           PIR_IsWarmingUp() ? "WARMUP " : (is_attract ? "ATTRACT" : "IDLE   "),
		           pg4, pir_trig_count);
		  Debug_Print(msg);
	  }
#elif DEMO_SCREENS
	  /* เดโมไล่ดูหน้าจอ เปลี่ยนเองทุก 3 วิ หรือกดปุ่มเกมดวงที่ 1 / แตะจอข้าม */
	  static int page = 0;
	  static int btn_was = 0, tp_was = 0;
	  static uint32_t last_change = 0;
	  int next = 0;

	  int btn = (HAL_GPIO_ReadPin(BTN_1_GPIO_Port, BTN_1_Pin) == GPIO_PIN_RESET);
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
	  PIR_Update();
	  Game_Tick();
#if DEBUG_PIR_LEDS
	  PirLeds_Update();
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
#if TEST_PIR_ONLY
static UART_HandleTypeDef dbg_huart3;

static void Debug_Init(void)
{
  GPIO_InitTypeDef g = {0};
  RCC_PeriphCLKInitTypeDef pclk = {0};

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

#if DEBUG_PIR_LEDS
/* ไฟ LD1-LD3 บนบอร์ด Nucleo (PB0 PB7 PB14) ไม่มีใครใช้บนบอร์ด A จึงตั้งขาเองตรงนี้ ไม่ต้องแก้ .ioc */
static void PirLeds_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  g.Pin  = GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_14;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOB, &g);
}

static void PirLeds_Update(void)
{
  int pa4   = (HAL_GPIO_ReadPin(PIR_OUT_GPIO_Port, PIR_OUT_Pin) == GPIO_PIN_SET);
  int awake = (__HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1) > 100);   /* IDLE หรี่ไฟจอเหลือ 5% */
  int blink = PIR_IsWarmingUp() && (HAL_GetTick() / 300) % 2;

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  pa4   ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* LD1 เขียว = ขา PA4 สด */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,  awake ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* LD2 ฟ้า  = จอตื่น */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, blink ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* LD3 แดง = วอร์มอัป */
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
