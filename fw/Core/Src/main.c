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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "sensor.h"
#include "dac.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
 TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart2_rx;

Sensor_t wind_sensor;
uint8_t sensor_rx_buf[256];
uint8_t poll_requested = 0;
/* USER CODE BEGIN PV */
uint32_t last_valid_data_tick = 0;  /* For 2-second sensor timeout */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void PrintSystemStatus(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)
  {
    // Low priority: LED blink (PA9)
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);
  }
  else if (htim->Instance == TIM2)
  {
    // Request a step in sensor logic
    poll_requested = 1;
  }
}

uint32_t system_msg_id = 0;

void PrintSystemStatus(void)
{
  system_msg_id++;

  uint8_t is_timeout = (HAL_GetTick() - last_valid_data_tick > 2000);
  const char *sensor_status = is_timeout ? "ER" : "OK";

  float spd_mA = 3.5f;
  float dir_mA = 3.5f;

  float spd_val = 0.0f;
  float dir_val = 0.0f;

  if (!is_timeout)
  {
    spd_val = wind_sensor.speed;
    dir_val = wind_sensor.direction;

    spd_mA = 4.0f + (spd_val / 60.0f) * 16.0f;
    if (spd_mA > 20.0f) spd_mA = 20.0f;
    if (spd_mA < 4.0f)  spd_mA = 4.0f;

    dir_mA = 4.0f + (dir_val / 360.0f) * 16.0f;
    if (dir_mA > 20.0f) dir_mA = 20.0f;
    if (dir_mA < 4.0f)  dir_mA = 4.0f;
  }

  /* Read DAC status registers via SPI */
  uint16_t st1 = DAC_ReadStatus(DAC_CHANNEL_SPEED);
  uint16_t st2 = DAC_ReadStatus(DAC_CHANNEL_DIRECTION);

  /* Read nERR hardware pins */
  uint8_t dac_err = DAC_ReadErrors();
  const char *nerr1_str = (dac_err & 0x01) ? "ER" : "OK";
  const char *nerr2_str = (dac_err & 0x02) ? "ER" : "OK";

  char ms_buf[256];
  snprintf(ms_buf, sizeof(ms_buf),
           "%lu. |  VEL: %05.2f | %06.3f mA | DIR: %05.1f | %06.3f mA | S: %s %s %s | SPI %04X %04X |\r\n",
           system_msg_id, spd_val, spd_mA, dir_val, dir_mA,
           sensor_status, nerr1_str, nerr2_str, st1, st2);
  HAL_UART_Transmit(&huart3, (uint8_t *)ms_buf, strlen(ms_buf), 200);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  HAL_Delay(100); // Safety delay for debugger attach
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  Sensor_Init(&wind_sensor, 1); // Initialize sensor with ID 1

  /* Enable UART3 (Debug) RS-485 Transmission */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);   // DE = 1
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // nRE = 0

  /* Force UART2 to 9600 for Sensor */
  huart2.Init.BaudRate = 9600;
  HAL_UART_Init(&huart2);

  /* Start UART2 DMA Reception */
  HAL_UART_Receive_DMA(&huart2, sensor_rx_buf, sizeof(sensor_rx_buf));

  /* Initialize DAC outputs — both channels start at 3.5 mA (error level) */
  DAC_Init(&hspi1);

  last_valid_data_tick = HAL_GetTick();

  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_Base_Start_IT(&htim3);

  /* Send startup message */
  char *msg = "\r\n--- WindTrans System Started ---\r\n";
  HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), 100);

  /* ---- SPI self-check ---- */
  {
    char dbg[200];

    /* 1. Echo/loopback probe: write 0xA55A, read back echo next frame.
     *    If MISO floats HIGH  -> 0xFFFF  (broken MISO path)
     *    If MISO floats LOW   -> 0x0000  (MISO stuck low)
     *    If SPI OK            -> 0xA55A  (echo matches) */
    uint16_t p1 = DAC_SpiProbe(DAC_CHANNEL_SPEED,     0xA55A);
    uint16_t p2 = DAC_SpiProbe(DAC_CHANNEL_DIRECTION, 0xA55A);

    /* 2. STATUS register read (valid only if probe returned 0xA55A) */
    uint16_t st1 = DAC_ReadStatus(DAC_CHANNEL_SPEED);
    uint16_t st2 = DAC_ReadStatus(DAC_CHANNEL_DIRECTION);

    snprintf(dbg, sizeof(dbg),
             "INIT: nERR=0x%02X\r\n"
             "  DAC1: probe=0x%04X %s  status=0x%04X\r\n"
             "  DAC2: probe=0x%04X %s  status=0x%04X\r\n",
             DAC_ReadErrors(),
             p1, (p1 == 0xA55A) ? "[SPI OK ]" : (p1 == 0xFFFF) ? "[MISO HI]" : "[SPI ERR]", st1,
             p2, (p2 == 0xA55A) ? "[SPI OK ]" : (p2 == 0xFFFF) ? "[MISO HI]" : "[SPI ERR]", st2);
    HAL_UART_Transmit(&huart3, (uint8_t *)dbg, strlen(dbg), 200);
  }

  /* ---- DAC channel self-test (rapid-write, no timeout possible) ----
   * Each step writes the target current every 10ms for 5 seconds.
   * At 10ms interval, SPI timeout (100ms) cannot fire.
   * If ammeter still doesn't change -> SPI signals not reaching DAC (ADUM issue). */
#if 0
  {
    char tb[48];
    uint32_t t0;

    #define BLAST_MS 5000   /* 5 seconds per step */
    #define BLAST_INTERVAL 10  /* write every 10ms */

    /* DAC1 = 4 mA */
    HAL_UART_Transmit(&huart3, (uint8_t *)"TEST DAC1=4mA (5s)...\r\n", 23, 100);
    t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < BLAST_MS) {
      DAC_SetCurrent(DAC_CHANNEL_SPEED, 4.0f);
      HAL_Delay(BLAST_INTERVAL);
    }

    /* DAC1 = 20 mA */
    HAL_UART_Transmit(&huart3, (uint8_t *)"TEST DAC1=20mA (5s)...\r\n", 24, 100);
    t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < BLAST_MS) {
      DAC_SetCurrent(DAC_CHANNEL_SPEED, 20.0f);
      HAL_Delay(BLAST_INTERVAL);
    }

    /* DAC2 = 4 mA */
    DAC_SetError();
    HAL_UART_Transmit(&huart3, (uint8_t *)"TEST DAC2=4mA (5s)...\r\n", 23, 100);
    t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < BLAST_MS) {
      DAC_SetCurrent(DAC_CHANNEL_DIRECTION, 4.0f);
      HAL_Delay(BLAST_INTERVAL);
    }

    /* DAC2 = 20 mA */
    HAL_UART_Transmit(&huart3, (uint8_t *)"TEST DAC2=20mA (5s)...\r\n", 24, 100);
    t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < BLAST_MS) {
      DAC_SetCurrent(DAC_CHANNEL_DIRECTION, 20.0f);
      HAL_Delay(BLAST_INTERVAL);
    }

    snprintf(tb, sizeof(tb), "TEST done. nERR=0x%02X\r\n", DAC_ReadErrors());
    HAL_UART_Transmit(&huart3, (uint8_t *)tb, strlen(tb), 100);

    /* Flush sensor DMA buffer accumulated during test */
    DAC_SetError();
    memset(sensor_rx_buf, 0, sizeof(sensor_rx_buf));
    HAL_UART_AbortReceive(&huart2);
    HAL_UART_Receive_DMA(&huart2, sensor_rx_buf, sizeof(sensor_rx_buf));
    last_valid_data_tick = HAL_GetTick();
  }
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 1. Handle outgoing requests (1Hz)
    if (poll_requested)
    {
      poll_requested = 0;
      Sensor_Step(&wind_sensor, &huart2);
    }

    // 2. Handle incoming data
    uint16_t curr_pos = sizeof(sensor_rx_buf) - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    if (curr_pos > 0) 
    {
      /* Wait for packet to fully arrive — but keep DAC alive every 50ms.
       * HAL_Delay(300) was blocking DAC_Refresh and causing SPI timeout (100ms). */
      {
        uint32_t wait_start = HAL_GetTick();
        while (HAL_GetTick() - wait_start < 300)
        {
          DAC_Refresh();
          HAL_Delay(50);
        }
      }
      curr_pos = sizeof(sensor_rx_buf) - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
      if (curr_pos >= sizeof(sensor_rx_buf)) curr_pos = sizeof(sensor_rx_buf) - 1;
      sensor_rx_buf[curr_pos] = '\0';

      // Always show RAW for debugging
      HAL_UART_Transmit(&huart3, (uint8_t *)"RAW: ", 5, 10);
      HAL_UART_Transmit(&huart3, sensor_rx_buf, curr_pos, 100);
      HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, 10);

      int res = Sensor_Parse(&wind_sensor, (char*)sensor_rx_buf);
      if (res == 1) // Data parsed successfully
      {
        /* Update DAC outputs */
        DAC_UpdateOutputs(wind_sensor.speed, wind_sensor.direction);
        last_valid_data_tick = HAL_GetTick();
        PrintSystemStatus();
      }
      else if (res == 2) // Version received during init
      {
        HAL_UART_Transmit(&huart3, (uint8_t *)"MS: Sensor Verified! Switching to Poll mode.\r\n", 46, 100);
        wind_sensor.state = SENSOR_READY_POLL;
      }

      memset(sensor_rx_buf, 0, sizeof(sensor_rx_buf));
      HAL_UART_AbortReceive(&huart2);
      HAL_UART_Receive_DMA(&huart2, sensor_rx_buf, sizeof(sensor_rx_buf));
    }

    /* Sensor timeout check: 2000 ms without valid data -> NAMUR NE43 error */
    static uint32_t last_timeout_print_tick = 0;
    if (HAL_GetTick() - last_valid_data_tick > 2000)
    {
      DAC_SetError();
      if (HAL_GetTick() - last_timeout_print_tick >= 2000)
      {
        last_timeout_print_tick = HAL_GetTick();
        PrintSystemStatus();
      }
    }

    /* DAC keepalive: refresh every 50 ms to prevent SPI timeout watchdog */
    static uint32_t dac_refresh_tick = 0;
    if (HAL_GetTick() - dac_refresh_tick >= 50)
    {
      dac_refresh_tick = HAL_GetTick();
      DAC_Refresh();
    }

    HAL_Delay(10);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 7199;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_13|GPIO_PIN_14, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 PA4 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB13 PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB2 (nERR inputs) */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/**
  * @brief SPI1 Initialization Function (PA5=SCK, PA6=MISO, PA7=MOSI)
  *        Mode 1: CPOL=0, CPHA=1 as required by DAC161S997
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance               = SPI1;
  hspi1.Init.Mode              = SPI_MODE_MASTER;
  hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL=0 */
  hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA=0 (DAC161S997 clocks on rising edge) */
  hspi1.Init.NSS               = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; /* 72MHz/64 = 1.125 MHz */
  hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial     = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
