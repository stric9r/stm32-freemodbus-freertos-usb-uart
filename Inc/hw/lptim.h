/**
  ******************************************************************************
  * @file    lptim.h
  * @brief   This file contains all the function prototypes for
  *          the lptim.c file
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

#ifndef __LPTIM_H__
#define __LPTIM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"


void lptim_init(void);
void lptim_set_period(uint8_t timerNum, uint16_t timerUs);
void lptim_enable(uint8_t timerNum);
void lptim_disable(uint8_t timerNum);

#ifdef __cplusplus
}
#endif

#endif /* __LPTIM_H__ */

