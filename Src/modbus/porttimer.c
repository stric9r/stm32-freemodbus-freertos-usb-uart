#include "mb.h"
#include "mbport.h"
#include "port_internal.h"

/**
 * @brief  Initialize the FreeModbus hardware timer.
 *
 * Initializes MB_TIMER and configures its auto-reload period from
 * @p usTim1Timerout50us. No hardware clock is enabled here; that is deferred
 * to vMBPortTimersEnable() so the timer only consumes power while a Modbus
 * frame is actively being received.
 *
 * @param  usTim1Timerout50us  Timeout expressed in 50 µs units, as supplied
 *                             by the FreeModbus stack (derived from baud rate).
 * @return TRUE always (assert fires on HAL failure inside MB_TIMER_ENABLE).
 */
BOOL xMBPortTimersInit( USHORT usTim1Timerout50us )
{
    uint16_t const timeUs = (uint16_t)((uint32_t)usTim1Timerout50us * 50u);

    MB_TIMER_INIT();
    MB_TIMER_SET_PERIOD(timeUs);

    return TRUE;
}

/**
 * @brief  Arm the t3.5 silence-detection timer.
 *
 * Called by FreeModbus at the start of every received character to (re)start
 * the inter-frame gap countdown. Brings up MB_TIMER — gating its clock on —
 * and launches a one-shot countdown. If the timer was already running from
 * the previous character, MB_TIMER_ENABLE() tears it down and restarts it,
 * extending the silence window as required by the Modbus spec.
 */
void vMBPortTimersEnable( void )
{
    MB_TIMER_ENABLE();
}

/**
 * @brief  Disarm the t3.5 silence-detection timer.
 *
 * Called by FreeModbus once a complete frame has been received or an error
 * has been detected. Stops the countdown and gates the timer clock off so
 * the peripheral draws no dynamic power until the next frame arrives.
 */
void vMBPortTimersDisable( void )
{
    MB_TIMER_DISABLE();
}

/**
 * @brief  Modbus timer interrupt handler.
 */
void MB_TIMER_IRQ( void )
{
    MB_TIMER_IRQ_FUNC();
}
