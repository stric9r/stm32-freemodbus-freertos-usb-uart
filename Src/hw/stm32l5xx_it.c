/**
  ******************************************************************************
  * @file    stm32l5xx_it.c
  * @brief   Interrupt Service Routines.
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

#include "main.h"
#include "stm32l5xx_it.h"

extern PCD_HandleTypeDef hpcd_USB_FS;

// @todo [2026-04-26] Use DMA or remove it
//extern DMA_HandleTypeDef hdma_usart2_tx;

// Not using here, modbus handles it directly outside of FreeRTOS and CubeMX
//extern UART_HandleTypeDef huart2;

extern TIM_HandleTypeDef htim6;

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/

/******************************************************************************/
/* STM32L5xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l5xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 channel3 global interrupt.
  */
void DMA1_Channel3_IRQHandler(void)
{
  // @todo [2026-04-26] Use DMA or remove it
  // HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/**
  * @brief This function handles USART2 global interrupt / USART2 wake-up interrupt through EXTI line 27.
  */
 /* Not using here, modbus handles it directly outside of FreeRTOS and CubeMX
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}*/

/**
  * @brief This function handles USB FS global interrupt / USB FS wake-up interrupt through EXTI line 34.
  */
void USB_FS_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}
