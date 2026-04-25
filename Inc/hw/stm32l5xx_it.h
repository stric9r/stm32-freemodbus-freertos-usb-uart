/**
  ******************************************************************************
  * @file    stm32l5xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
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
#ifndef __STM32L5xx_IT_H
#define __STM32L5xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void DMA1_Channel3_IRQHandler(void);
void TIM6_IRQHandler(void);
void USART2_IRQHandler(void);
void USB_FS_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32L5xx_IT_H */
