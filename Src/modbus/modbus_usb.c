/**
 * @file modbus_usb.c
 * @brief USB CDC transport adapter for the FreeModbus RTU stack.
 *
 * Bridges the USB CDC byte stream into the FreeModbus RTU state machine so
 * that a single eMBPoll() loop in modbus_task handles frames from both
 * USART2 and USB CDC without any duplicate Modbus parsing logic.
 *
 * ## How it works
 *
 * RX — frame accumulation:
 *   modbus_usb_run() blocks on usbRxStream (a FreeRTOS stream buffer filled
 *   by CDC_Receive_FS in ISR context) until the first byte of a frame arrives.
 *   Remaining bytes are drained with a 2 ms silence timeout — identical to the
 *   t3.5 inter-frame gap at typical baud rates.  All bytes land in frameBuf[].
 *
 * Pre-validation:
 *   Before touching the FreeModbus state machine, the frame is checked for
 *   minimum length, slave address match, and CRC validity.  This gate is
 *   necessary to prevent leaving bUsbActive stuck at true if FreeModbus
 *   would later reject the frame (FreeModbus does not call vMBPortSerialEnable
 *   on a CRC failure, so the port would never restore to UART automatically).
 *
 * Injection:
 *   After claiming port ownership, vMBPortSetUsbActive(true) redirects all
 *   FreeModbus byte I/O to USB CDC (see portserial.c).  The UART RX interrupt
 *   is masked so UART bytes cannot interleave with the injected bytes.
 *   vMBPortUsbInjectFrame() feeds each byte through xMBRTUReceiveFSM() (via
 *   pxMBFrameCBByteReceived) and then fires a synthetic t3.5 expiry to post
 *   EV_FRAME_RECEIVED — exactly what LPTIM1 does after a real UART frame.
 *
 * Response TX:
 *   eMBPoll() in modbus_task picks up EV_FRAME_RECEIVED, validates CRC
 *   (passes — we pre-checked), dispatches the function-code handler, then calls
 *   eMBRTUSend() which calls vMBPortSerialEnable(FALSE, TRUE).  Because
 *   bUsbActive is still true, that function drives xMBRTUTransmitFSM()
 *   synchronously, flushes the USB TX buffer via CDC_Transmit_FS(), and resets
 *   bUsbActive back to false — restoring UART automatically.
 *
 * ## What is NOT here
 *
 * There is no FC dispatch, no CRC computation for the response, no response
 * building.  FreeModbus handles all of that through the existing callbacks in
 * modbus_task.c (eMBRegHoldingCB, eMBRegInputCB, etc.) which in turn use
 * the shared modbus_mem register bank.  Adding a new function code requires
 * only the standard FreeModbus extension mechanism — no changes here.
 */

#include "modbus_usb.h"
#include "modbus_mem.h"
#include "port_addresses.h"
#include "portserial_usb.h"
#include "modbus_port_ownership.h"
#include "mbcrc.h"

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "task.h"

#include "port_internal.h"  /* MB_SERIAL_DISABLE_RX_IRQ() */

#include <stddef.h>
#include <stdbool.h>

/* Serial PDU layout: | addr (1) | PDU (N) | CRC-lo | CRC-hi | */
#define SER_PDU_SIZE_MAX    256u
#define SER_PDU_ADDR_OFF    0u

/* Minimum valid frame: addr + FC + 4 data bytes + 2 CRC = 8 bytes */
#define SER_PDU_SIZE_MIN    8u

static uint8_t slaveAddr;

/**
 * @brief Initialise the USB Modbus adapter.
 *
 * Creates the RX stream buffer (via portserial_usb_init) and records the
 * slave address.  Must be called before MX_USB_Device_Init() so the stream buffer
 * exists when the first CDC_Receive_FS ISR fires.
 */
void modbus_usb_init(void)
{
    portserial_usb_init();
    slaveAddr = DEFAULT_SLAVE_ADDR;
}

/**
 * @brief Run one USB receive → inject → respond cycle.
 *
 * Blocks at usbRxStream with portMAX_DELAY until the first byte of a frame
 * arrives (usb_task is fully idle when no USB data is present).  Drains
 * remaining bytes with a 2 ms silence timeout (t3.5 equivalent for USB).
 *
 * Pre-validates the frame (length, address, CRC) before touching the FreeModbus
 * state machine.  On success, injects the frame via the port layer mux so
 * eMBPoll() processes it as if it arrived from UART.
 *
 * A single bContinue flag controls flow through sequential checks so that
 * this function has exactly one return point.  Silent-discard conditions
 * (bad address, bad CRC, not owner) leave errorCode as MB_ENOERR.  Only
 * the portMAX_DELAY timeout path returns MB_ETIMEDOUT.
 *
 * @return MB_ENOERR   Frame injected, or silently discarded (not owner /
 *                     pre-validation failed).
 * @return MB_ETIMEDOUT Should not occur with portMAX_DELAY; defensive return
 *                     for the initial stream buffer receive.
 */
eMBErrorCode modbus_usb_run(void)
{
    uint8_t      frameBuf[SER_PDU_SIZE_MAX];
    uint16_t     frameLen    = 0u;
    uint8_t      byte        = 0u;
    eMBErrorCode errorCode   = MB_ENOERR;
    bool         bContinue   = true;

    /* Task sleeps here until CDC_Receive_FS ISR delivers the first byte */
    if (0u == xStreamBufferReceive(usbRxStream, &byte, 1u, portMAX_DELAY))
    {
        bContinue  = false;
        errorCode  = MB_ETIMEDOUT;
    }

    if (bContinue)
    {
        frameBuf[frameLen++] = byte;

        /* Drain remaining bytes; 2 ms silence = end of frame */
        while (frameLen < (uint16_t)SER_PDU_SIZE_MAX)
        {
            if (0u == xStreamBufferReceive(usbRxStream, &byte, 1u, pdMS_TO_TICKS(2u)))
            {
                break;
            }
            frameBuf[frameLen++] = byte;
        }
    }

    /* --- Pre-validation gate ---
     *
     * These checks MUST happen before calling vMBPortSetUsbActive() or
     * vMBPortUsbInjectFrame().  FreeModbus does not call vMBPortSerialEnable
     * when it rejects a frame (e.g. CRC failure), so if we injected a bad
     * frame the port layer would be left in USB mode permanently and UART
     * would stop responding.
     *
     * Note: FreeModbus performs its own CRC and address checks internally —
     * these pre-checks are a defensive duplicate to protect port state only.
     * Silent discard (errorCode stays MB_ENOERR) is the correct behaviour
     * for all three failure cases.                                           */
    if (bContinue && (frameLen < (uint16_t)SER_PDU_SIZE_MIN))
    {
        bContinue = false;  /* too short — discard silently */
    }

    if (bContinue && (frameBuf[SER_PDU_ADDR_OFF] != slaveAddr))
    {
        bContinue = false;  /* not our address — discard silently */
    }

    /* usMBCRC16 over the entire frame including the two CRC bytes must yield 0
     * for a valid frame.  The CRC bytes are appended little-endian by the master. */
    if (bContinue && (0u != usMBCRC16((UCHAR const *)frameBuf, frameLen)))
    {
        bContinue = false;  /* CRC mismatch — discard silently */
    }

    /* --- Port ownership ---
     *
     * In DYNAMIC mode: try to claim the bus.  If UART already owns it
     * (e.g. a UART master is actively polling) the frame is dropped.
     *
     * Why we do NOT claim ownership on the first byte: USB CDC delivers a
     * complete Modbus frame as a single CDC packet.  By the time
     * xStreamBufferReceive returns the first byte, all remaining bytes are
     * already in the stream buffer.  Claiming early would require an explicit
     * release on validation failure, adding complexity with no real benefit. */
     bContinue &= modbus_port_ownership_try_claim();

    /* --- Inject frame into FreeModbus via port layer mux ---
     *
     * 1. Redirect FreeModbus byte I/O to USB CDC.
     * 2. Mask UART RX interrupt so UART bytes cannot enter xMBRTUReceiveFSM()
     *    while USB bytes are being injected.
     * 3. Feed bytes through the RTU state machine byte-by-byte; fire synthetic
     *    t3.5 at the end → EV_FRAME_RECEIVED posted to eMBPoll() event queue.
     *
     * After this function returns, eMBPoll() in modbus_task will wake up,
     * process the frame, and send the response.  vMBPortSerialEnable(TRUE,FALSE)
     * at the end of that TX cycle will automatically:
     *   - clear bUsbActive (restoring UART routing), and
     *   - re-enable UART RX interrupt.
     * So the caller does NOT need to call vMBPortSetUsbActive(false).          */
    if (bContinue)
    {
        vMBPortSetUsbActive(true);
        MB_SERIAL_DISABLE_RX_IRQ();

        vMBPortUsbInjectFrame(frameBuf, frameLen);

        /* Reset the 500ms inactivity timer so ownership is not released mid-session */
        modbus_port_ownership_refresh();
    }

    return errorCode;
}
