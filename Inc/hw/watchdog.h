/**
 * @file watchdog.h
 * @brief Public interface for the IWDG hardware watchdog driver.
 *
 * Call watchdog_init() once from main() before starting the FreeRTOS
 * scheduler, then call watchdog_pet() at least every CHECK_IN_TIME_MS
 * milliseconds from each monitored task.
 *
 * The Modbus task achieves this by waking from its 60-second
 * xQueueReceive timeout in xMBPortEventGet() and calling watchdog_pet()
 * through a tick-gate in its polling loop.
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum interval between watchdog_pet() calls, in milliseconds.
 *
 * Must be less than the hardware IWDG timeout configured in watchdog.c.
 * Matches PORT_EVENT_TIMEOUT_MS in portevent.c so the Modbus task always
 * wakes in time to pet the watchdog even on an idle bus.
 *
 * Note the watchdog is configured to bark at ~32.8 seconds
 */
#define WD_CHECK_IN_TIME_MS   20000u

/**
 * @brief Initialise and start the IWDG hardware watchdog.
 *
 * Must be called from main() before vTaskStartScheduler().  The IWDG
 * cannot be stopped after this call; watchdog_pet() must be called
 * periodically or the MCU will reset.
 */
void watchdog_init(void);

/**
 * @brief Refresh the IWDG counter (pet the watchdog).
 *
 * Reloads the IWDG down-counter, preventing a watchdog reset.  Call at
 * an interval no greater than CHECK_IN_TIME_MS.
 */
void watchdog_pet(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_H */
