/**
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
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

/* Ascii Conversion ERROR*/
#define ASCII_CONV_ERR      0xFF

static uint8_t slaveAddr;

static uint8_t ascii_hex_nibble(uint8_t const c);

/**
 * @brief Decode one ASCII hex character to its 4-bit binary value.
 *
 * @return 0-15 for valid hex digits '0'-'9', 'A'-'F'.
 * @return ASCII_CONV_ERR for any other character (invalid).
 */
static uint8_t ascii_hex_nibble(uint8_t const c)
{
    uint8_t result = ASCII_CONV_ERR;
    if ((c >= '0') && (c <= '9')) 
    { 
        result = c - '0';
    }
    else if ((c >= 'A') && (c <= 'F')) 
    { 
        result = (uint8_t)(c - 'A' + 10u); 
    }

    return result;
}

/**
 * @brief Validate a Modbus ASCII frame received over USB CDC.
 *
 * Verifies frame structure, decodes the hex-pair encoding to binary,
 * checks the decoded slave address against @p slaveAddr, and validates
 * the LRC checksum.
 *
 * In Modbus ASCII the first byte is always ':'.  The slave address is
 * therefore at encoded positions [1..2], not at frameBuf[0].
 *
 * @param frameBuf   Raw bytes from the USB CDC stream, frameBuf[0] == ':'.
 * @param frameLen   Total byte count including ':' and CRLF.
 * @param slaveAddr  Expected slave address (binary, not ASCII-encoded).
 * @return true  Frame is structurally valid, addressed to this slave,
 *               and the LRC matches.
 * @return false Frame is malformed, addressed elsewhere, or LRC mismatch.
 */
static bool ascii_frame_is_valid(uint8_t const * const frameBuf,
                                 uint16_t const        frameLen,
                                 uint8_t const         slaveAddr)
{
    bool bValid = (frameLen >= 9u)
               && ('\r' == (char)frameBuf[frameLen - 2u])
               && ('\n' == (char)frameBuf[frameLen - 1u]);

    if (bValid)
    {
        /* Hex payload sits between ':' and CRLF — must be an even char count */
        uint16_t const hex_len = frameLen - 3u;   /* minus ':', CR, LF */
        bValid = (0u == (hex_len & 1u)) && (hex_len >= 4u);

        if (bValid)
        {
            uint16_t const bin_len      = hex_len / 2u;
            uint8_t        lrc_sum      = 0u;
            uint8_t        decoded_addr = 0u;

            /* Decode hex pairs; accumulate LRC over all bytes except the last */
            for (uint16_t i = 0u; bValid && (i < bin_len); i++)
            {
                uint8_t const hi = ascii_hex_nibble(frameBuf[1u + (i * 2u)]);
                uint8_t const lo = ascii_hex_nibble(frameBuf[2u + (i * 2u)]);
                bValid = (ASCII_CONV_ERR != hi) && (ASCII_CONV_ERR != lo);

                if (bValid)
                {
                    uint8_t const decoded = (uint8_t)((hi << 4u) | lo);
                    if (0u == i)
                    { 
                        decoded_addr = decoded; 
                    }
                    if (i < (bin_len - 1u)) 
                    { 
                        lrc_sum += decoded; 
                    }
                }
            }

            if (bValid)
            {
                bValid = (decoded_addr == slaveAddr);
            }

            if (bValid)
            {
                uint8_t const hi_lrc = ascii_hex_nibble(
                    frameBuf[1u + ((bin_len - 1u) * 2u)]);
                uint8_t const lo_lrc = ascii_hex_nibble(
                    frameBuf[2u + ((bin_len - 1u) * 2u)]);
                uint8_t const stored_lrc   = (uint8_t)((hi_lrc << 4u) | lo_lrc);
                uint8_t const computed_lrc = (uint8_t)(-(int8_t)lrc_sum);
                bValid = (stored_lrc == computed_lrc);
            }
        }
    }

    return bValid;
}

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
    slaveAddr = mb_mem_get_config()->slaveAddr;
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
     * Frames starting with ':' are Modbus ASCII — validated with LRC and
     * decoded slave address.  All other frames are treated as Modbus RTU —
     * validated with RTU CRC and raw slave address byte.
     *
     * Silent discard (errorCode stays MB_ENOERR) is correct for all failure
     * cases; only the portMAX_DELAY timeout returns MB_ETIMEDOUT.           */
    if (bContinue && (frameLen < (uint16_t)SER_PDU_SIZE_MIN))
    {
        bContinue = false;  /* too short — discard silently */
    }

    if (bContinue)
    {
        if (':' == (char)frameBuf[0])
        {
            /* ASCII frame — validate structure, slave address, and LRC */
            bContinue = ascii_frame_is_valid(frameBuf, frameLen, slaveAddr);
        }
        else
        {
            /* RTU frame — validate slave address and CRC */
            if (frameBuf[SER_PDU_ADDR_OFF] != slaveAddr)
            {
                bContinue = false;  /* not our address — discard silently */
            }

            /* usMBCRC16 over the entire frame including the two CRC bytes
             * must yield 0 for a valid RTU frame.                          */
            if (bContinue && (0u != usMBCRC16((UCHAR const *)frameBuf, frameLen)))
            {
                bContinue = false;  /* CRC mismatch — discard silently */
            }
        }
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
