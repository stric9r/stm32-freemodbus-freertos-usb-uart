/**
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
/**
 * @file watchdog.c
 * @brief IWDG (Independent Watchdog) driver for STM32L5.
 *
 * Drives the IWDG directly via CMSIS register access — no HAL IWDG driver
 * required.  The IWDG runs off the internal LSI oscillator (~32 kHz) and
 * cannot be stopped once started.
 *
 * Hardware constraint — STM32L5 IWDG with LSI ≈ 32 kHz:
 *   max timeout = (prescaler × (reload_max + 1)) / LSI
 *               = (256 × 4096) / 32 000
 *               ≈ 32.8 s
 *
 * WATCHDOG_TIMEOUT_MS is the design intent (120 s); the actual hardware
 * timeout is capped at ~32.8 s because the reload register is 12-bit
 * (max 4095).  IWDG_RELOAD_VAL is computed from WATCHDOG_TIMEOUT_MS but
 * saturated to 0xFFF so the register write is always valid.  Adjust
 * CHECK_IN_TIME_MS in watchdog.h to stay within the real hardware window.
 */

#include "watchdog.h"

#if WD_DEBUG_BLUE 
#include "gpio.h"
#include "main.h"
#endif

#include "stm32l5xx_hal.h"   /* RCC LSI enable, CMSIS device header */


#include <assert.h>
#include <stdbool.h>


static IWDG_HandleTypeDef hiwdg;
static bool bInitialized = false;

/**
 * @brief Initialise and start the IWDG hardware watchdog.
 */
void watchdog_init(void)
{
#ifdef DEBUG
    __HAL_DBGMCU_FREEZE_IWDG();
    __HAL_DBGMCU_FREEZE_WWDG();
#endif

  // We have an assumption that our clock is 32kHz
  assert(32000U == LSI_VALUE);

  // LSI_VALUE is defined in stm32l5xx_hal_conf.h (nominally 32 000 Hz).
  // At 32 kHz / 256 → 125 Hz tick rate:
  // Max achievable:  4096 / 125 = 32.768 s
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  
  assert(HAL_OK == HAL_IWDG_Init(&hiwdg));

  bInitialized = true;
}

/**
 * @brief Refresh the IWDG down-counter (pet the watchdog).
 */
void watchdog_pet(void)
{
    assert(bInitialized);

    HAL_IWDG_Refresh(&hiwdg);

#if WD_DEBUG_BLUE
    gpio_toggle_pin(GPIO_BLUE_LED_GPIO_Port, GPIO_BLUE_LED_Pin);
#endif
}
