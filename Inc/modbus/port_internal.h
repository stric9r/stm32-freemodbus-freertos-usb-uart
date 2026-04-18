#ifndef _PORT_INTERNAL_H
#define _PORT_INTERNAL_H

#include "stm32l5xx_hal.h"
#include "gpio.h"
#include "main.h"


/* Debug helper functions */
#if MB_TIMER_DEBUG == 1
static inline void vMBTimerDebugSetHigh( void )
{
    HAL_GPIO_WritePin(GPIO_RED_LED_GPIO_Port, GPIO_RED_LED_Pin, GPIO_PIN_SET);
}

static inline void vMBTimerDebugSetLow( void )
{
    HAL_GPIO_WritePin(GPIO_RED_LED_GPIO_Port, GPIO_RED_LED_Pin, GPIO_PIN_RESET );
}
#else
#define vMBTimerDebugSetHigh()
#define vMBTimerDebugSetLow()
#endif


#endif // _PORT_INTERNAL_H
