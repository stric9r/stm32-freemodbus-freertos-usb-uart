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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

// @todo [2026-4-12] Rename main.h to common.h or something similar

void Error_Handler(void);
void SystemClock_Config(void);


#define GPIO_BUTTON_Pin GPIO_PIN_13
#define GPIO_BUTTON_GPIO_Port GPIOC
#define GPIO_GREEN_LED_Pin GPIO_PIN_7
#define GPIO_GREEN_LED_GPIO_Port GPIOC
#define GPIO_RED_LED_Pin GPIO_PIN_9
#define GPIO_RED_LED_GPIO_Port GPIOA
#define GPIO_MB_USAT_RX_Pin GPIO_PIN_5
#define GPIO_MB_USAT_RX_GPIO_Port GPIOD
#define GPIO_MB_USART_RX_Pin GPIO_PIN_6
#define GPIO_MB_USART_RX_GPIO_Port GPIOD
#define GPIO_BLUE_LED_Pin GPIO_PIN_7
#define GPIO_BLUE_LED_GPIO_Port GPIOB



#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
