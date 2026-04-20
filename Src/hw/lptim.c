/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lptim.c
  * @brief   This file provides code for the configuration
  *          of the LPTIM instances.
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

#include "lptim.h"

#include <assert.h>
#include <stdbool.h>


static LPTIM_HandleTypeDef hLptim1;
static uint16_t periodTicksLptim1;
static bool bInitialized = false;

static void lptim1_init(void);
static uint32_t lptimPrescalerDiv(LPTIM_HandleTypeDef const * const hLptim);

/**
 * @brief  Configure the LPTIM1 HAL handle.
 *
 * Fills the handle struct with hardware settings for LPTIM1. 
 * No hardware registers are touched here and the peripheral clock 
 * remains off.
 *
 * Settings applied:
 *   - Clock source      : APB / LPOSC (PCLK1)
 *   - Prescaler         : DIV32  (tick = PCLK1 / 32)
 *   - Trigger           : software
 *   - Counter           : internal
 *   - RepetitionCounter : 0 (interrupt fires every period, no extra repetitions)
 */
static void lptim1_init(void)
{
  hLptim1.Instance = LPTIM1;
  hLptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hLptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV32;
  hLptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hLptim1.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
  hLptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hLptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hLptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hLptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  hLptim1.Init.RepetitionCounter = 0;
}

/**
 * @brief  Initialize all LPTIM peripherals.
 *
 * Calls the private per-instance init functions and marks the module as
 * initialized. lptim_set_period(), lptim_enable(), and lptim_disable() all
 * assert on bInitialized, so this must be called before any of them.
 *
 * No hardware clocks are enabled here; that is deferred to lptim_enable()
 * so peripherals only consume power when actively in use.
 */
void lptim_init(void)
{
  lptim1_init();
  bInitialized = true;
}

/**
 * @brief  Compute and store the auto-reload period for the given timer.
 *
 * Converts @p timerUs (microseconds) to LPTIM ticks using the live PCLK1
 * frequency and the prescaler configured in the HAL handle, then stores
 * the result as the ARR value used by lptim_enable().
 *
 * Must be called after lptim_init() and before lptim_enable().
 *
 * ARR formula: ticks = (timerUs * (PCLK1 / prescaler)) / 1 000 000 - 1
 *
 * @param  timerNum  Timer instance identifier (1 = LPTIM1).
 * @param  timerUs   Desired timeout in microseconds.
 */
void lptim_set_period(uint8_t const timerNum, uint16_t const timerUs)
{
  assert(bInitialized);

  if (1u == timerNum)
  {
    uint32_t const lptimTickHz = HAL_RCC_GetPCLK1Freq() / lptimPrescalerDiv(&hLptim1);
    periodTicksLptim1 =
        (uint16_t)(((uint32_t)timerUs * (lptimTickHz / 1000u)) / 1000u) - 1u;
  }
  else
  {
    bool const bLpTimerNotSupported = false;
    assert(bLpTimerNotSupported);
  }
}

/**
 * @brief  Arm or re-arm the given LPTIM timer for a one-shot countdown.
 *
 * Counts up from 0 to the ARR value stored by lptim_set_period() and fires
 * the LPTIM IRQ exactly once when it reaches that value.
 *
 * If the timer is already running (e.g. called again before the previous
 * countdown expired), the current count is discarded and a fresh countdown
 * begins from 0 — no Init/DeInit cycle is needed because the peripheral
 * clock is already gated on.
 *
 * If the timer is not running (first call after lptim_disable()), a full
 * HAL_LPTIM_Init() is performed to gate the peripheral clock on before
 * starting the countdown.
 *
 * Asserts if the module has not been initialized or the timer is not supported.
 *
 * @param  timerNum  Timer instance identifier (1 = LPTIM1).
 */
void lptim_enable(uint8_t const timerNum)
{
  assert(bInitialized);

  if (1u == timerNum)
  {
    if (HAL_LPTIM_STATE_BUSY == hLptim1.State)
    {
      // Timer already running — stop it so we can restart from 0
      HAL_LPTIM_OnePulse_Stop_IT(&hLptim1);
    }
    else
    {
      // Coming from a cold/disabled state — bring up peripheral clock and NVIC
      assert(HAL_OK == HAL_LPTIM_Init(&hLptim1));
    }
    HAL_LPTIM_OnePulse_Start_IT(&hLptim1, periodTicksLptim1, 0u);
  }
  else
  {
    bool const bLpTimerNotSupported = false;
    assert(bLpTimerNotSupported);
  }
}

/**
 * @brief  Stop the given LPTIM timer and gate its clock off.
 *
 * Cancels any in-progress countdown then calls HAL_LPTIM_DeInit(), which
 * triggers HAL_LPTIM_MspDeInit() and disables the peripheral clock. The
 * timer draws no dynamic power until the next lptim_enable() call.
 *
 * Safe to call even if the countdown has already expired.
 *
 * Asserts if the module has not been initialized or the timer is not supported.
 *
 * @param  timerNum  Timer instance identifier (1 = LPTIM1).
 */
void lptim_disable(uint8_t const timerNum)
{
  assert(bInitialized);

  if (1u == timerNum)
  {
    HAL_LPTIM_OnePulse_Stop_IT(&hLptim1);
    HAL_LPTIM_DeInit(&hLptim1);
  }
  else
  {
    bool const bLpTimerNotSupported = false;
    assert(bLpTimerNotSupported);
  }
}

/**
 * @brief  MSP initialization callback for LPTIM peripherals.
 *
 * Overrides the weak implementation in stm32l5xx_hal_lptim.c. Configures
 * the peripheral clock source to PCLK1 and enables the LPTIM1 clock.
 * Called automatically by HAL_LPTIM_Init() — do not call directly.
 *
 * Asserts if called with an unsupported LPTIM instance.
 *
 * @param  lptimHandle  Pointer to the LPTIM handle being initialized.
 */
void HAL_LPTIM_MspInit(LPTIM_HandleTypeDef* lptimHandle)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if (LPTIM1 == lptimHandle->Instance)
  {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
    PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_PCLK1;
    assert(HAL_OK == HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit));

    __HAL_RCC_LPTIM1_CLK_ENABLE();

    HAL_NVIC_SetPriority(LPTIM1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
  }
  else
  {
    bool const bLpTimerNotSupported = false;
    assert(bLpTimerNotSupported);
  }
}

/**
 * @brief  MSP de-initialization callback for LPTIM peripherals.
 *
 * Overrides the weak implementation in stm32l5xx_hal_lptim.c. Disables
 * the LPTIM1 peripheral clock, removing its dynamic power contribution.
 * Called automatically by HAL_LPTIM_DeInit() — do not call directly.
 *
 * Asserts if called with an unsupported LPTIM instance.
 *
 * @param  lptimHandle  Pointer to the LPTIM handle being de-initialized.
 */
void HAL_LPTIM_MspDeInit(LPTIM_HandleTypeDef* lptimHandle)
{
  if (LPTIM1 == lptimHandle->Instance)
  {
    __HAL_RCC_LPTIM1_CLK_DISABLE();
  }
  else
  {
    bool const bLpTimerNotSupported = false;
    assert(bLpTimerNotSupported);
  }
}

/**
 * @brief  Return the integer clock divisor for the prescaler in the given handle.
 *
 * Maps the raw CFGR PRESC field value stored in the HAL handle to its numeric
 * divisor, so the tick frequency can be computed without a hardcoded magic
 * number. Asserts on an unrecognised prescaler value.
 *
 * @param  hLptim  Pointer to the LPTIM handle to read.
 * @return Clock divisor: 1, 2, 4, 8, 16, 32, 64, or 128.
 */
static uint32_t lptimPrescalerDiv(LPTIM_HandleTypeDef const * const hLptim)
{
  assert(NULL != hLptim);

  uint32_t prescaler = 1u;

  switch (hLptim->Init.Clock.Prescaler)
  {
    case LPTIM_PRESCALER_DIV1:   prescaler = 1u;   break;
    case LPTIM_PRESCALER_DIV2:   prescaler = 2u;   break;
    case LPTIM_PRESCALER_DIV4:   prescaler = 4u;   break;
    case LPTIM_PRESCALER_DIV8:   prescaler = 8u;   break;
    case LPTIM_PRESCALER_DIV16:  prescaler = 16u;  break;
    case LPTIM_PRESCALER_DIV32:  prescaler = 32u;  break;
    case LPTIM_PRESCALER_DIV64:  prescaler = 64u;  break;
    case LPTIM_PRESCALER_DIV128: prescaler = 128u; break;
    default:
    {
      bool const bInvalidPrescaler = false;
      assert(bInvalidPrescaler);
    }
  }

  return prescaler;
}
