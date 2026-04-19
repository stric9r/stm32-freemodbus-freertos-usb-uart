/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"


void gpio_init(void);
void gpio_set_pin(GPIO_TypeDef * const port, uint16_t const pin);
void gpio_clear_pin(GPIO_TypeDef * const port, uint16_t const pin);
void gpio_toggle_pin(GPIO_TypeDef * const port, uint16_t const  pin);

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

