/**
 * @file portserial.c
 * @brief FreeModbus serial port layer — UART and USB CDC transport mux.
 *
 * ## Original UART path (unchanged behaviour)
 *
 * FreeModbus owns three port-layer functions that abstract all byte I/O:
 *
 *   xMBPortSerialGetByte()  — called inside xMBRTUReceiveFSM() to read the
 *                             byte that just caused the USART2 RX interrupt.
 *                             Reads directly from USART2->RDR.
 *
 *   xMBPortSerialPutByte()  — called inside xMBRTUTransmitFSM() to write the
 *                             next response byte. Writes directly to USART2->TDR.
 *                             Driven byte-by-byte by the USART2 TXE interrupt.
 *
 *   vMBPortSerialEnable()   — called by FreeModbus to switch between receive and
 *                             transmit modes by gating USART2_CR1_RXNEIE and
 *                             USART2_CR1_TXEIE independently.  On RS-485 this
 *                             also controls the direction pin; here it ensures
 *                             only one interrupt is armed at a time.
 *
 * The UART ISR (MB_SERIAL_PERIPH_IRQ / USART2_IRQHandler) calls
 * pxMBFrameCBByteReceived (= xMBRTUReceiveFSM) for each RX byte and
 * pxMBFrameCBTransmitterEmpty (= xMBRTUTransmitFSM) for each TX ready event.
 * These are global function pointers set by eMBInit().
 *
 * ## USB transport mux (new)
 *
 * When bUsbActive is true the three port functions are redirected:
 *
 *   xMBPortSerialGetByte()  — returns usbPendingByte (the byte being injected
 *                             by vMBPortUsbInjectFrame) instead of reading USART2.
 *
 *   xMBPortSerialPutByte()  — accumulates into the USB TX buffer via
 *                             portserial_usb_put_byte() instead of writing TDR.
 *
 *   vMBPortSerialEnable()   — TX start path: drives xMBRTUTransmitFSM()
 *                             synchronously (no TXE interrupt needed for USB),
 *                             flushes the CDC TX buffer, then fires a synthetic
 *                             t3.5 expiry so FreeModbus completes its TX cycle.
 *                           — TX end path: resets bUsbActive and re-enables
 *                             UART RX so UART resumes after the USB response.
 *
 * The injection function vMBPortUsbInjectFrame() feeds a validated USB CDC
 * frame byte-by-byte through xMBRTUReceiveFSM() and then fires pxMBPortCBTimerExpired
 * to post EV_FRAME_RECEIVED — exactly what LPTIM1 would do after t3.5 silence
 * on UART.  eMBPoll() in modbus_task picks up that event and processes the
 * frame through the normal FreeModbus callback chain (CRC already verified by
 * the caller, so eMBRTUReceive() will also pass it; slave address checked too).
 *
 * ## t3.5 timer suppression during injection
 *
 * xMBRTUReceiveFSM() calls vMBPortTimersEnable() after every byte to restart
 * the LPTIM1 countdown.  During injection the timer must not fire between bytes
 * (it would post a premature EV_FRAME_RECEIVED).  vMBPortTimersEnable() in
 * porttimer.c checks xMBPortIsUsbActive() and skips MB_TIMER_ENABLE() when
 * true.  After each injected byte vMBPortTimersDisable() is called explicitly
 * as a belt-and-braces guard.
 *
 * ## Concurrency
 *
 * bUsbActive is set by the USB task (priority 4).  It is read by the UART
 * task (priority 5) inside vMBPortSerialEnable().  Because the UART task is
 * higher priority, it will not pre-empt until the USB task blocks or yields —
 * which happens only after vMBPortUsbInjectFrame() has finished posting
 * EV_FRAME_RECEIVED.  At that point bUsbActive is true and the UART task
 * sees a consistent flag throughout processing + TX.  bUsbActive is reset
 * to false inside vMBPortSerialEnable(TRUE,FALSE) which runs in the UART task
 * context, so the USB task cannot see the old value during reset.
 */

#include "mb.h"
#include "mbport.h"
#include "port.h"
#include "port_internal.h"
#include "portserial_usb.h"

#if USB_MODBUS_ACTIVE_DEBUG_GREEN
#include "gpio.h"
#include "main.h"
#endif

#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Transport mux state
 * -----------------------------------------------------------------------*/

/**
 * When true, all three FreeModbus port functions (GetByte, PutByte, Enable)
 * route to USB CDC instead of USART2.  Set by vMBPortSetUsbActive() from the
 * usb_task before frame injection.  Cleared inside vMBPortSerialEnable(TRUE,
 * FALSE) which FreeModbus calls at the end of each TX cycle — so it is
 * automatically restored to false after the USB response is sent.
 */
static volatile bool bUsbActive = false;

/**
 * Staging register for the byte currently being injected into
 * xMBRTUReceiveFSM().  xMBPortSerialGetByte() reads this instead of
 * USART2->RDR when bUsbActive is true.  Written immediately before each
 * call to pxMBFrameCBByteReceived() in vMBPortUsbInjectFrame().
 */
static CHAR usbPendingByte = 0;

/* -------------------------------------------------------------------------
 * Transport mux API (declared in portserial_usb.h)
 * -----------------------------------------------------------------------*/

/**
 * @brief Expose the USB-active flag to porttimer.c so vMBPortTimersEnable()
 *        can skip arming LPTIM1 during frame injection.
 */
bool xMBPortIsUsbActive(void)
{
    return bUsbActive;
}

/**
 * @brief Activate or deactivate USB transport routing.
 *
 * Call with active=true from the usb_task before calling
 * vMBPortUsbInjectFrame().  The flag is cleared automatically by
 * vMBPortSerialEnable(TRUE, FALSE) at the end of the FreeModbus TX cycle,
 * so callers do not need to call vMBPortSetUsbActive(false) explicitly.
 *
 * @param active  true to route FreeModbus I/O to USB CDC; false to restore UART.
 */
void vMBPortSetUsbActive(bool active)
{
    bUsbActive = active;
    
#if USB_MODBUS_ACTIVE_DEBUG_GREEN
    HAL_GPIO_WritePin(GPIO_GREEN_LED_GPIO_Port, GPIO_GREEN_LED_Pin,
                      active ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

/**
 * @brief Inject a validated USB CDC frame into the FreeModbus RTU state machine.
 *
 * Feeds each byte from @p pFrame one at a time into xMBRTUReceiveFSM() via the
 * pxMBFrameCBByteReceived function pointer — exactly as the UART ISR would do
 * for bytes arriving from USART2.  After the last byte, fires
 * pxMBPortCBTimerExpired() to simulate the t3.5 inter-frame silence, which
 * posts EV_FRAME_RECEIVED to the FreeModbus event queue.
 *
 * After this call returns, eMBPoll() in modbus_task will pick up
 * EV_FRAME_RECEIVED, validate CRC (which must already pass — only call this
 * with a CRC-valid, address-matched frame), dispatch the function-code handler,
 * and send the response through vMBPortSerialPutByte() / vMBPortSerialEnable()
 * — both of which are redirected to USB when bUsbActive is true.
 *
 * t3.5 suppression: xMBRTUReceiveFSM() calls vMBPortTimersEnable() after each
 * byte to restart LPTIM1.  vMBPortTimersEnable() is a no-op while
 * xMBPortIsUsbActive() returns true (guard in porttimer.c).  As an extra
 * safety net, vMBPortTimersDisable() is called after each injection to cancel
 * any arm that slipped through a race.
 *
 * @param pFrame  Pointer to the raw RTU frame (addr + PDU + CRC).
 * @param len     Total frame length in bytes.
 */
void vMBPortUsbInjectFrame(uint8_t const * pFrame, uint16_t len)
{
    for (uint16_t i = 0u; i < len; i++)
    {
        /* Stage the byte so xMBPortSerialGetByte() can return it when
         * xMBRTUReceiveFSM() calls it on the very next line.            */
        usbPendingByte = (CHAR)pFrame[i];

        /* Equivalent to the UART ISR calling pxMBFrameCBByteReceived().
         * xMBRTUReceiveFSM() reads usbPendingByte via xMBPortSerialGetByte()
         * and appends it to ucRTUBuf[], then calls vMBPortTimersEnable()
         * which is suppressed by the xMBPortIsUsbActive() guard.         */
        (void)pxMBFrameCBByteReceived();

        /* Belt-and-braces: disable LPTIM1 in case vMBPortTimersEnable()
         * managed to arm it between the FSM call and this line.          */
        vMBPortTimersDisable();
    }

    /* Simulate t3.5 inter-frame silence.  pxMBPortCBTimerExpired()
     * (= xMBRTUTimerT35Expired) sees the state machine in STATE_RX_RCV,
     * posts EV_FRAME_RECEIVED to the FreeModbus event queue, and transitions
     * to STATE_RX_IDLE — identical to what LPTIM1 does after a real UART frame.
     * portevent.c uses xPortIsInsideInterrupt() to select xQueueSend (task
     * context) vs xQueueSendFromISR, so calling this from task context is safe. */
    (void)pxMBPortCBTimerExpired();
}

/* -------------------------------------------------------------------------
 * FreeModbus port functions — UART path documented, USB redirect noted
 * -----------------------------------------------------------------------*/

/**
 * @brief  Initialize the FreeModbus serial port (UART only).
 *
 * Converts the FreeModbus parity enum to the numeric value expected by the
 * usart layer (0 = none, 1 = odd, 2 = even), then initializes USART2 with
 * the requested line parameters. The @p ucPORT argument is unused because
 * the hardware USART is fixed by MB_SERIAL in port_internal.h.
 *
 * Called once by eMBInit() before the FreeRTOS scheduler starts.
 *
 * @param  ucPORT      Unused — hardware port is fixed (USART2).
 * @param  ulBaudRate  Baud rate in bits per second.
 * @param  ucDataBits  Number of data bits: 7, 8, or 9.
 * @param  eParity     FreeModbus parity enum (MB_PAR_NONE/ODD/EVEN).
 * @param  ucStopBits  Number of stop bits: 1 or 2.
 * @return TRUE always.
 */
BOOL xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits,
                        eMBParity eParity, UCHAR ucStopBits )
{
    (void)ucPORT;

    uint8_t parity = 0u;
    parity = (MB_PAR_ODD  == eParity) ? 1u : parity;
    parity = (MB_PAR_EVEN == eParity) ? 2u : parity;

    MB_SERIAL_INIT((uint32_t)ulBaudRate, (uint8_t)ucDataBits,
                   (uint8_t)ucStopBits, parity);

    MB_SERIAL_ENABLE_RX_IRQ();

    return TRUE;
}

/**
 * @brief  Gate the USART2 RX/TX interrupts independently — or handle USB TX.
 *
 * ## UART path (bUsbActive == false)
 *
 * Called by FreeModbus to switch between receive and transmit modes.  On
 * half-duplex RS-485 this controls the bus direction pin by arming only one
 * interrupt at a time:
 *
 *   rxEnable=TRUE,  txEnable=FALSE — start of reception or end of TX cycle.
 *                                    Arms USART2_CR1_RXNEIE, masks TXEIE.
 *   rxEnable=FALSE, txEnable=TRUE  — start of transmission.
 *                                    Arms USART2_CR1_TXEIE, masks RXNEIE.
 *
 * The TXEIE interrupt fires once per byte-slot and is handled in
 * MB_SERIAL_PERIPH_IRQ via MB_SERIAL_IRQ_FUNC().  Each firing calls
 * pxMBFrameCBTransmitterEmpty() (= xMBRTUTransmitFSM()) which calls
 * xMBPortSerialPutByte() to write the next byte to TDR.  When all bytes
 * are sent the FSM calls vMBPortTimersEnable() to arm the post-TX t3.5
 * silence window, then vMBPortSerialEnable(TRUE, FALSE) is called again
 * by xMBRTUTimerT35Expired() to restore RX mode.
 *
 * ## USB TX path (bUsbActive == true, rxEnable=FALSE, txEnable=TRUE)
 *
 * There is no TXEIE interrupt for USB CDC — the response must be sent as a
 * single CDC transfer.  This branch drives xMBRTUTransmitFSM() synchronously
 * in a tight loop until all response bytes have been consumed by
 * xMBPortSerialPutByte() (which redirects to portserial_usb_put_byte()).
 * Then portserial_usb_flush_tx() fires CDC_Transmit_FS() in one shot.
 *
 * After the flush, pxMBPortCBTimerExpired() is called to simulate the post-TX
 * t3.5 silence.  xMBRTUTimerT35Expired() sees the state machine in
 * STATE_TX_XFWR and calls vMBPortSerialEnable(TRUE, FALSE) — which is the
 * "USB restore" branch below.
 *
 * ## USB restore path (bUsbActive == true, rxEnable=TRUE, txEnable=FALSE)
 *
 * Called by xMBRTUTimerT35Expired() after the TX cycle completes.  Resets
 * bUsbActive to false so subsequent FreeModbus calls go back to UART, and
 * re-enables the UART RX interrupt so UART frames can be received again.
 *
 * @param  rxEnable  TRUE to arm the RX interrupt; FALSE to mask it.
 * @param  txEnable  TRUE to arm the TX interrupt; FALSE to mask it.
 */
void vMBPortSerialEnable( BOOL rxEnable, BOOL txEnable )
{
    if (bUsbActive)
    {
        if (!rxEnable && txEnable)
        {
            /* --- USB TX start ---
             * Drive the RTU transmit state machine synchronously.
             * xMBRTUTransmitFSM() (pxMBFrameCBTransmitterEmpty) returns TRUE
             * while there are bytes remaining and FALSE when done (STATE_TX_XFWR).
             * Each call invokes xMBPortSerialPutByte() which accumulates bytes
             * in the USB TX buffer via portserial_usb_put_byte().              */
            while (pxMBFrameCBTransmitterEmpty()) { }

            /* Send all accumulated bytes as one CDC bulk transfer */
            portserial_usb_flush_tx();

            /* Simulate post-TX t3.5 silence.  xMBRTUTimerT35Expired() sees
             * STATE_TX_XFWR, resets the RTU state machine to IDLE, and calls
             * vMBPortSerialEnable(TRUE, FALSE) — which hits the branch below.  */
            (void)pxMBPortCBTimerExpired();
        }
        else if (rxEnable && !txEnable)
        {
            /* --- USB restore (called from within pxMBPortCBTimerExpired above) ---
             * TX cycle is complete.  Clear the USB flag so the next FreeModbus
             * operation goes back to UART hardware.  Re-enable the UART RX
             * interrupt that was masked before frame injection began.            */
            bUsbActive = false;
            MB_SERIAL_ENABLE_RX_IRQ();
        }
        /* Any other combination (both true/false) is not issued by FreeModbus
         * during normal operation — fall through and return without touching
         * UART hardware, since UART RX was intentionally masked.                */
        return;
    }

    /* --- UART path --- */
    if (rxEnable)
    {
        MB_SERIAL_ENABLE_RX_IRQ();
    }
    else
    {
        MB_SERIAL_DISABLE_RX_IRQ();
    }

    if (txEnable)
    {
        MB_SERIAL_ENABLE_TX_IRQ();
    }
    else
    {
        MB_SERIAL_DISABLE_TX_IRQ();
    }
}

/**
 * @brief  Write one byte to the transmit data register — or to USB TX buffer.
 *
 * ## UART path (bUsbActive == false)
 * Writes directly to USART2->TDR, bypassing the HAL to avoid per-byte overhead
 * on the hot transmit path.  Called from xMBRTUTransmitFSM() which is itself
 * called from the USART2 TXE interrupt handler.
 *
 * ## USB path (bUsbActive == true)
 * Appends the byte to the USB TX accumulation buffer via portserial_usb_put_byte().
 * Called in the synchronous drive loop inside vMBPortSerialEnable(FALSE, TRUE).
 * portserial_usb_flush_tx() sends the full buffer as one CDC transfer after the
 * loop completes.
 *
 * @param  byte  Byte to transmit.
 * @return TRUE always.
 */
BOOL xMBPortSerialPutByte( CHAR byte )
{
    if (bUsbActive)
    {
        portserial_usb_put_byte((uint8_t)byte);
        return TRUE;
    }
    MB_SERIAL_PUT_BYTE(byte);
    return TRUE;
}

/**
 * @brief  Read one byte from the receive data register — or from the USB pending slot.
 *
 * ## UART path (bUsbActive == false)
 * Reads directly from USART2->RDR, bypassing the HAL to avoid per-byte overhead
 * on the hot receive path.  Called from xMBRTUReceiveFSM() which is itself
 * invoked by the USART2 RXNE interrupt handler immediately after a byte arrives.
 * The byte is guaranteed to be present in RDR at the point of this call.
 *
 * ## USB path (bUsbActive == true)
 * Returns usbPendingByte which was staged by vMBPortUsbInjectFrame()
 * immediately before calling pxMBFrameCBByteReceived().  This makes the RTU
 * state machine believe it is reading from USART2 — the source of the byte is
 * transparent to xMBRTUReceiveFSM().
 *
 * @param  byte  Pointer to store the received byte.
 * @return TRUE always.
 */
BOOL xMBPortSerialGetByte( CHAR *byte )
{
    if (bUsbActive)
    {
        *byte = usbPendingByte;
        return TRUE;
    }
    *byte = (CHAR)MB_SERIAL_GET_BYTE();
    return TRUE;
}

/**
 * @brief  USART2 hardware interrupt handler.
 *
 * Owns the USART2_IRQHandler vector-table entry.  HAL_UART_IRQHandler() is
 * intentionally NOT called here — the HAL dispatcher runs ~15 flag checks,
 * dispatches through function pointers, and manages DMA bookkeeping that are
 * all irrelevant to the FreeModbus byte-at-a-time model.
 *
 * MB_SERIAL_IRQ_FUNC() (port_internal.h) reads USART2->ISR once, clears the
 * three error flags that would stall the UART (framing, noise, overrun), and
 * calls the FreeModbus callbacks only for the interrupt sources that are
 * actually enabled in CR1:
 *
 *   RXNE + RXNEIE  → pxMBFrameCBByteReceived() = xMBRTUReceiveFSM()
 *   TXE  + TXEIE   → pxMBFrameCBTransmitterEmpty() = xMBRTUTransmitFSM()
 *
 * Note: This handler fires for UART bytes only.  When bUsbActive is true the
 * UART RX interrupt has been masked (RXNEIE cleared), so this ISR will not
 * interfere with USB frame injection.
 */
void MB_SERIAL_PERIPH_IRQ( void )
{
    MB_SERIAL_IRQ_FUNC();
}
