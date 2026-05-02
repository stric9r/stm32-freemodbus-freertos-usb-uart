/**
 * @file modbus_port_ownership.c
 * @brief Dynamic Modbus port ownership arbitration.
 *
 * When COMMS_MODBUS_PORT == COMMS_MODBUS_DYNAMIC, whichever transport (UART or
 * USB) processes a complete frame first claims ownership via a binary semaphore.
 * Ownership is held until @c modbus_port_ownership_refresh() is not called for
 * PORT_OWNER_TIMEOUT_MS milliseconds — the one-shot timer then fires and
 * automatically releases the semaphore, allowing either transport to claim it
 * on the next frame.
 *
 * Ownership is checked and refreshed at the frame level (once per completed
 * transaction), not per-byte, to avoid flooding the FreeRTOS timer queue.
 *
 * Binary semaphore is used instead of a mutex because the timer callback
 * releases the semaphore without having taken it.  This is valid for binary
 * semaphores but is undefined behaviour for mutexes.  Priority inheritance is
 * not needed — the UART task (priority 5) already outranks the USB task
 * (priority 4) and will win naturally in the rare case of simultaneous frames.
 *
 * In COMMS_MODBUS_UART or COMMS_MODBUS_USB builds, all three functions compile
 * to no-ops or trivial stubs so callers need no #if guards.
 */

#include "modbus_port_ownership.h"

#include "portserial.h"
#include "portserial_usb.h"

#if COMMS_MODBUS_PORT == COMMS_MODBUS_DYNAMIC

#include "FreeRTOS.h"
#include "semphr.h"
#include "timers.h"

#define PORT_OWNER_TIMEOUT_MS   500u

static StaticSemaphore_t portOwnerSemBuf;
static SemaphoreHandle_t portOwnerSem;

static StaticTimer_t     portOwnerTimerBuf;
static TimerHandle_t     portOwnerTimer;

/**
 * @brief Timer callback — fires after PORT_OWNER_TIMEOUT_MS of inactivity.
 *
 * Gives the binary semaphore back without having taken it, which is valid for
 * binary semaphores and releases port ownership so either transport can claim
 * it on the next incoming frame.
 */
static void port_owner_timeout_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    (void)xSemaphoreGive(portOwnerSem);
    vMBPortSetUsbActive(false);
}

#endif /* COMMS_MODBUS_DYNAMIC */

/**
 * @brief Initialise the port ownership mechanism.
 *
 * Creates the binary semaphore (initially available) and the one-shot
 * inactivity timer.  Must be called from @c system_app_init() before the
 * scheduler starts.  In non-DYNAMIC builds this is a no-op.
 */
void modbus_port_ownership_init(void)
{
#if COMMS_MODBUS_PORT == COMMS_MODBUS_DYNAMIC
    portOwnerSem = xSemaphoreCreateBinaryStatic(&portOwnerSemBuf);
    (void)xSemaphoreGive(portOwnerSem);   /* start available — no owner */

    portOwnerTimer = xTimerCreateStatic(
        "mbOwner",
        pdMS_TO_TICKS(PORT_OWNER_TIMEOUT_MS),
        pdFALSE,   /* one-shot */
        NULL,
        port_owner_timeout_cb,
        &portOwnerTimerBuf);
#endif
}

/**
 * @brief Attempt to claim port ownership (non-blocking, fail-fast).
 *
 * In DYNAMIC builds: tries @c xSemaphoreTake with timeout=0.  Returns true
 * if ownership was granted; false if another port already owns the bus —
 * the caller should discard the frame silently.
 *
 * In single-port builds: always returns true (ownership is implicit).
 *
 * @return true   Ownership claimed — call @c modbus_port_ownership_refresh()
 *                after the response is sent.
 * @return false  Another port owns the bus — discard this frame.
 */
bool modbus_port_ownership_try_claim(void)
{
#if COMMS_MODBUS_PORT == COMMS_MODBUS_DYNAMIC
    return (pdTRUE == xSemaphoreTake(portOwnerSem, 0));
#else
    return true;
#endif
}

/**
 * @brief Reset the inactivity timer after a completed transaction.
 *
 * Call once per completed frame + response pair, not per byte.  If this
 * function is not called within PORT_OWNER_TIMEOUT_MS, the timer fires and
 * releases ownership automatically.
 *
 * In non-DYNAMIC builds this is a no-op.
 */
void modbus_port_ownership_refresh(void)
{
#if COMMS_MODBUS_PORT == COMMS_MODBUS_DYNAMIC
    (void)xTimerReset(portOwnerTimer, 0);
#endif
}
