#ifndef __ASSERT_PROJ_H
#define __ASSERT_PROJ_H


#ifdef USE_FULL_ASSERT
#include "cmsis_compiler.h"
#else
#include <assert.h>
#endif

#include "stm32l5xx_hal.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


// Keep this prototype around so bringing in new functionality
// from STM32CubeMX doesn't break us.
void Error_Handler(void);

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT

// Redirect C standard assert() to HAL assert_param().
// With USE_FULL_ASSERT enabled, assert_param(expr) calls assert_failed(file, line)
// which calls Error_Handler() when a debugger is attached.
#ifdef assert
#undef assert
#endif

#define assert(expr) assert_param(expr)

volatile bool isDebuggerAttached(void) 
{
  return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0;
}

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    // @todo [2026-4-12] Print to a serial debug port

    while(isDebuggerAttached())
    {
      __NOP();
    }

    Error_Handler();
}
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif /* __ASSERT_PROJ_H */