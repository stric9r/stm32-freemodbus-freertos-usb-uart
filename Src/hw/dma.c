/**
  ******************************************************************************
  * @file    dma.c
  * @brief   This file provides code for the configuration
  *          of all the requested memory to memory DMA transfers.
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
/*
 * Modified by Stric Roberts, 2026.
 * MIT License — see LICENSE in the project root.
 */
#include "dma.h"

/**
  * Enable DMA controller clock
  */
void dma_init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  //HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
  //HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

