/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

#include "gpio.h"

#include <assert.h>

#include <stdbool.h>
#include <stddef.h>


static bool bInitialized = false;

/**
 * @brief Initialise all GPIO pins for the application.
 *
 * Enables port clocks, sets unused pins to analog (no-pull) to minimise
 * current draw, and configures the active pins listed below.
 *
 * Active pin assignments:
 *   - PC14 / PC15      OSC32_IN / OSC32_OUT  — 32 kHz LSE crystal
 *   - PH0  / PH1       OSC_IN  / OSC_OUT     — HSE crystal
 *   - GPIO_GREEN_LED   Push-pull output, low speed, initially low
 *   - GPIO_RED_LED     Push-pull output, low speed, initially low
 *   - GPIO_BLUE_LED    Push-pull output, low speed, initially low
 *   - GPIO_BUTTON      Input, no pull
 *
 * Must be called from main() after HAL_Init() and SystemClock_Config().
 */
void gpio_init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIO_GREEN_LED_GPIO_Port, GPIO_GREEN_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIO_RED_LED_GPIO_Port, GPIO_RED_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIO_BLUE_LED_GPIO_Port, GPIO_BLUE_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2 PE3 PE4 PE5
                           PE6 PE7 PE8 PE9
                           PE10 PE11 PE12 PE13
                           PE14 PE15 PE0 PE1 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : GPIO_BUTTON_Pin */
  GPIO_InitStruct.Pin = GPIO_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIO_BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PF0 PF1 PF2 PF3
                           PF4 PF5 PF6 PF7
                           PF8 PF9 PF10 PF11
                           PF12 PF13 PF14 PF15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PC0 PC1 PC2 PC3
                           PC6 PC8 PC9 PC10
                           PC11 PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3
                           PA4 PA5 PA6 PA7
                           PA8 PA10 PA15 */
  // Leave PA13 and PA14 alone, as they are SWDIO and SWCLK respectively
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_8|GPIO_PIN_10|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB10
                           PB11 PB13 PB14 PB15
                           PB4 PB5 PB6
                           PB8 PB9 */
  // Leave PB3 alone as its SWO SB140 on the board needs to be shorted to use this
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PG0 PG1 PG2 PG3
                           PG4 PG5 PG6 PG7
                           PG8 PG9 PG10 PG12
                           PG13 PG14 PG15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : PD8 PD9 PD10 PD11
                           PD12 PD13 PD14 PD15
                           PD0 PD1 PD2 PD3
                           PD4 PD7 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : GPIO_GREEN_LED_Pin */
  GPIO_InitStruct.Pin = GPIO_GREEN_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIO_GREEN_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : GPIO_RED_LED_Pin */
  GPIO_InitStruct.Pin = GPIO_RED_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIO_RED_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : GPIO_BLUE_LED_Pin */
  GPIO_InitStruct.Pin = GPIO_BLUE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIO_BLUE_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PH3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  bInitialized = true;
}

/**
 * @brief Drive a GPIO pin high.
 *
 * Thin wrapper around HAL_GPIO_WritePin() using GPIO_PIN_SET.
 *
 * @param port  Pointer to the GPIO peripheral (e.g. GPIOA, GPIOB).
 * @param pin   Pin bitmask (e.g. GPIO_RED_LED_Pin).
 */
void gpio_set_pin(GPIO_TypeDef * const port, uint16_t const pin)
{
  assert(NULL != port);
  assert(bInitialized);
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

/**
 * @brief Drive a GPIO pin low.
 *
 * Thin wrapper around HAL_GPIO_WritePin() using GPIO_PIN_RESET.
 *
 * @param port  Pointer to the GPIO peripheral (e.g. GPIOA, GPIOB).
 * @param pin   Pin bitmask (e.g. GPIO_RED_LED_Pin).
 */
void gpio_clear_pin(GPIO_TypeDef *const port, uint16_t const  pin)
{
  assert(NULL != port);
  assert(bInitialized);
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

/**
 * @brief Toggle a GPIO pin output state.
 *
 * Thin wrapper around HAL_GPIO_TogglePin().
 *
 * @param port  Pointer to the GPIO peripheral (e.g. GPIOA, GPIOB).
 * @param pin   Pin bitmask (e.g. GPIO_BLUE_LED_Pin).
 */
void gpio_toggle_pin(GPIO_TypeDef * const port, uint16_t const pin)
{
  assert(NULL != port);
  assert(bInitialized);
  HAL_GPIO_TogglePin(port, pin);
}
