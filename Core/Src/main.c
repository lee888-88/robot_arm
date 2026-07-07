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
#include "soft_i2c.h"
#include "ssd1306.h"
#include <stdio.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/* USER CODE BEGIN PV */
volatile uint16_t adc_values[4];  /* DMA circular buffer: [0]=PA0, [1]=PA1, [2]=PA2, [3]=PA3 */
uint32_t adc_filtered[4] = {0};  /* EMA filtered values */
TIM_HandleTypeDef htim1;          /* TIM1: 4-ch PWM, PA8-11 */
TIM_HandleTypeDef htim3;          /* TIM3: 1-ch PWM, PA6 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
static void OLED_DisplayData(void);
static void MX_TIM1_PWM_Init(void);
static void MX_TIM3_PWM_Init(void);
uint32_t map_range(uint32_t value, uint32_t in_min, uint32_t in_max,
                   uint32_t out_min, uint32_t out_max);
uint32_t pulse_to_angle(uint32_t pulse_us);
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
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  /* Calibrate ADC (required by STM32F1 datasheet) */
  HAL_ADCEx_Calibration_Start(&hadc1);

  /* Start ADC continuous scan with DMA circular mode — 4 channels */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_values, 4);

  /* Wait for OLED VDD to stabilize (~100ms) */
  HAL_Delay(100);

  /* Initialize software I2C and OLED display */
  soft_i2c_init();
  ssd1306_init();
  ssd1306_clear();

  /* Initialize TIM1 + TIM3 PWM for 5 servo channels (50Hz) */
  MX_TIM1_PWM_Init();
  MX_TIM3_PWM_Init();

  /* Start all 5 PWM channels */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  /* PA8  — base    */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);  /* PA9  — shoulder */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);  /* PA10 — elbow   */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);  /* PA11 — wrist   */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);  /* PA6  — hand    */

  /* Initialize all servos: 4 joints to 90° mid, gripper closed */
  SERVO_BASE(SERVO_MID_PULSE);
  SERVO_SHOULDER(SERVO_MID_PULSE);
  SERVO_ELBOW(SERVO_MID_PULSE);
  SERVO_WRIST(SERVO_MID_PULSE);
  SERVO_HAND(HAND_CLOSE_PULSE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ── EMA filter: α = 1/8 ── */
    for (int i = 0; i < 4; i++)
        adc_filtered[i] = (adc_filtered[i] * 7 + adc_values[i]) / 8;

    /* ── Filtered ADC → servo pulse (reversed: ADC↑ = pulse↓ = angle↓) ── */
    uint32_t pulse_base     = map_range(adc_filtered[0], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
    uint32_t pulse_shoulder = map_range(adc_filtered[1], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
    uint32_t pulse_elbow    = map_range(adc_filtered[2], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
    uint32_t pulse_wrist    = map_range(adc_filtered[3], 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);

    /* Update 4 joint servos */
    SERVO_BASE(pulse_base);
    SERVO_SHOULDER(pulse_shoulder);
    SERVO_ELBOW(pulse_elbow);
    SERVO_WRIST(pulse_wrist);

    /* Gripper button: PA4 pressed (LOW) → close, released (HIGH) → open */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
        SERVO_HAND(HAND_CLOSE_PULSE);   /* 879μs — close gripper */
    } else {
        SERVO_HAND(HAND_OPEN_PULSE);    /* 439μs — open gripper */
    }

    /* Update OLED display (~10Hz) */
    OLED_DisplayData();
    HAL_Delay(100);

    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief  Linear value mapping (equivalent to Arduino map())
 */
uint32_t map_range(uint32_t value, uint32_t in_min, uint32_t in_max,
                          uint32_t out_min, uint32_t out_max)
{
    /*
     * Use int64_t for intermediate calculation to handle:
     * 1. Reverse mapping where out_max < out_min (negative delta)
     * 2. Large multiplications that could overflow uint32_t
     */
    if (in_max == in_min) return out_min;

    if (value < in_min) value = in_min;
    if (value > in_max) value = in_max;

    return (uint32_t)(((int64_t)(value - in_min) * ((int64_t)out_max - (int64_t)out_min))
                      / (int64_t)(in_max - in_min) + (int64_t)out_min);
}

/**
 * @brief  Convert pulse width in μs to angle in degrees (650→0°, 2350→180°)
 */
uint32_t pulse_to_angle(uint32_t pulse_us)
{
    return map_range(pulse_us, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180);
}

/**
 * @brief  Read 4 ADC channels + switch state, render to OLED with servo angles
 *
 *         Display layout (128x64, each page = 8px, 21 chars/line max):
 *           Page 0 → Pot1: XXXX X.XXV NNN
 *           Page 1 → Pot2: XXXX X.XXV NNN
 *           Page 2 → Pot3: XXXX X.XXV NNN
 *           Page 3 → Pot4: XXXX X.XXV NNN
 *           Page 4 → SW: XXXXX Hand: XXX
 *
 *         NNN = servo angle 0-180 (pulse→angle).
 *         No clear() call — fixed-width overwrite eliminates flicker.
 */
static void OLED_DisplayData(void)
{
    char buf[22];   /* 21 chars + null */
    uint16_t raw;
    uint16_t mv;
    uint8_t  v_int, v_dec;
    uint32_t pulse;
    uint32_t angle;

    /* Potentiometer channels 1-4 on pages 0-3 */
    for (uint8_t i = 0; i < 4; i++) {
        raw = adc_filtered[i];

        /* Voltage: fixed-point integer to avoid newlib-nano %f issue */
        mv    = (uint32_t)raw * 3300U / 4096U;
        v_int = mv / 1000U;
        v_dec = (mv % 1000U) / 10U;

        /* Servo angle: ADC → pulse → angle (reversed mapping) */
        pulse = map_range(raw, 600, 3500, SERVO_MAX_PULSE, SERVO_MIN_PULSE);
        angle = pulse_to_angle(pulse);
        if (angle > 180) angle = 180;  /* clamp */

        snprintf(buf, sizeof(buf), "Pot%d:%-4d %d.%02dV %3lu",
                 i + 1, raw, v_int, v_dec, angle);

        /* Pad to exactly 21 chars: fill from \0 with spaces */
        for (uint8_t j = 0; j < 21; j++) {
            if (buf[j] == '\0') {
                while (j < 21) buf[j++] = ' ';
                buf[21] = '\0';
                break;
            }
        }
        ssd1306_set_cursor(i, 0);
        ssd1306_write_string(buf);
    }

    /* Switch + Hand state on page 4 (both driven by PA4 button) */
    {
        uint8_t  pressed = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET);
        /* "CLOSED"=6 chars, "OPEN"=4 chars; "CLS"/"OPN"=3 chars */
        snprintf(buf, sizeof(buf), "SW:%-6s Hand:%-3s",
                 pressed ? "CLOSED" : "OPEN",
                 pressed ? "CLS" : "OPN");
        for (uint8_t j = 0; j < 21; j++) {
            if (buf[j] == '\0') {
                while (j < 21) buf[j++] = ' ';
                buf[21] = '\0';
                break;
            }
        }
        ssd1306_set_cursor(4, 0);
        ssd1306_write_string(buf);
    }
}

/**
 * @brief  TIM1 PWM initialization — 4 channels, 50Hz, 1μs resolution
 *         PA8=CH1(base), PA9=CH2(shoulder), PA10=CH3(elbow), PA11=CH4(wrist)
 */
static void MX_TIM1_PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim1.Instance = TIM1;
    htim1.Init.Prescaler         = 72 - 1;     /* 72MHz / 72 = 1MHz (1μs/tick) */
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 20000 - 1;  /* 1MHz / 20000 = 50Hz */
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim1);                  /* → HAL_TIM_PWM_MspInit() callback */

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = SERVO_MID_PULSE;   /* initial: 1500μs = 90° */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4);
}

/**
 * @brief  TIM3 PWM initialization — 1 channel, 50Hz, 1μs resolution
 *         PA6=CH1(hand/gripper)
 */
static void MX_TIM3_PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler         = 72 - 1;     /* 72MHz / 72 = 1MHz */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 20000 - 1;  /* 1MHz / 20000 = 50Hz */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim3);                  /* → HAL_TIM_PWM_MspInit() callback */

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = SERVO_MID_PULSE;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);
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
