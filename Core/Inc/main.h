/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* ===== 舵机脉宽范围 (μs, 1计数值=1μs) ===== */
#define SERVO_MIN_PULSE     650
#define SERVO_MAX_PULSE     2350
#define SERVO_MID_PULSE     1500
#define HAND_OPEN_PULSE     439
#define HAND_CLOSE_PULSE    879

/* ===== 舵机控制宏：直接写 CCR 寄存器 ===== */
#define SERVO_BASE(p)       __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (p))
#define SERVO_SHOULDER(p)   __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (p))
#define SERVO_ELBOW(p)      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (p))
#define SERVO_WRIST(p)      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (p))
#define SERVO_HAND(p)       __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (p))

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern volatile uint16_t adc_values[4];

uint32_t map_range(uint32_t value, uint32_t in_min, uint32_t in_max,
                   uint32_t out_min, uint32_t out_max);
uint32_t pulse_to_angle(uint32_t pulse_us);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
