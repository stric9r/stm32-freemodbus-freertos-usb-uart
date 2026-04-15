/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"


// @todo [2026-4-12] Decouple how the handle is passed around.  It's ugly.
extern UART_HandleTypeDef huart2;


void usart_uart_init(uint8_t  const usartNum,
					           uint32_t const baudRate,
				             uint8_t  const dataBits,
                     uint8_t  const stop_bits,
					           uint8_t  const parity);

void usart_uart_bringup(uint8_t const usartNum);
void usart_uart_teardown(uint8_t const usartNum);

static inline void usart_uart2_set_byte(uint8_t const byte){ USART2->TDR = byte;};
static inline uint8_t usart_uart2_get_byte(void){ return USART2->RDR;};

UART_HandleTypeDef * usart_get_handle(uint8_t const usartNum);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

