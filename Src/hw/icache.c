/**
  ******************************************************************************
  * @file    icache.c
  * @brief   This file provides code for the configuration
  *          of the ICACHE instances.
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

#include "icache.h"

#include "assert.h"

/* ICACHE init function */
void icache_init(void)
{
  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  assert(HAL_OK == HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY));
  assert(HAL_OK == HAL_ICACHE_Enable());
}


