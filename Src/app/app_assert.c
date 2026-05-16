/**
  ******************************************************************************
  * @file           : app_assert.c
  * @brief          : Overrides newlib __assert_func to capture assert location
  *                   in NOINIT RAM before halting. The record survives a
  *                   watchdog reset so it can be inspected on next boot.
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#include "app_assert.h"
#include "stm32l5xx_hal.h"

assert_record_t g_assert_record __attribute__((section(".noinit")));

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    g_assert_record.magic = ASSERT_MAGIC;
    g_assert_record.file  = file;
    g_assert_record.line  = line;
    g_assert_record.func  = func;
    g_assert_record.expr  = expr;

    __disable_irq();
    if (0u != (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)) {
        __BKPT(0);
    }
    for (;;) {}
}
