/**
  ******************************************************************************
  * @file    usart_common.h
  * @brief   This file contains common function prototypes to 
  *          alleviate coupling
  ******************************************************************************
  */

#ifndef __USART_COMMON_H__
#define __USART_COMMON_H__

#include "stm32l5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void USART2_IRQRXHandler(UART_HandleTypeDef *huart);
void USART2_IRQTXHandler(UART_HandleTypeDef *huart);


#ifdef __cplusplus
}
#endif
#endif /*__ USART_COMMON_H__ */

