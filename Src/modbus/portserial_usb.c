/**
 * @file portserial_usb.c
 * @brief CDC byte-level serial interface for the USB Modbus port.
 *
 * Provides a thin byte-stream layer between the STM32 USB CDC stack and the
 * USB Modbus handler (@c modbus_usb.c).
 *
 * RX path (ISR → task):
 *   @c CDC_Receive_FS (USB ISR context) writes received bytes into @c usbRxStream
 *   via @c xStreamBufferSendFromISR.  The stream buffer decouples the ISR from
 *   the @c usb_task, which drains it at task priority.
 *
 * TX path (task → USB host):
 *   The Modbus handler calls @c portserial_usb_put_byte() for each byte of the
 *   response frame.  Bytes accumulate in @c usbTxBuf.  When the complete frame
 *   is ready, @c portserial_usb_flush_tx() sends it as a single CDC transfer.
 *   Sending one bulk transfer per frame is more efficient than firing one
 *   CDC_Transmit_FS per byte.
 */

#include "portserial_usb.h"
#include "usbd_cdc_if.h"

#include "FreeRTOS.h"
#include "stream_buffer.h"

#include <assert.h>

/* Sized for Modbus ASCII worst case: `:` + 2 hex chars per byte × (addr + FC + 252 data)
 * + 2-char LRC + CRLF = 513 bytes.  RTU worst case is only 256 bytes, but ASCII is
 * the larger frame format so sizing for ASCII covers both modes.                       */
#define USB_TX_BUF_SIZE     513u

static StaticStreamBuffer_t usbRxStreamStorage;
static uint8_t              usbRxStreamBuf[APP_RX_DATA_SIZE + 1u];
StreamBufferHandle_t        usbRxStream;

static uint8_t  usbTxBuf[USB_TX_BUF_SIZE];
static uint16_t usbTxLen;

/**
 * @brief Initialise the USB serial port layer.
 *
 * Creates the RX stream buffer.  Must be called before @c usb_device_init()
 * so the stream buffer exists when the first USB ISR fires.
 */
void portserial_usb_init(void)
{
    usbTxLen   = 0u;
    usbRxStream = xStreamBufferCreateStatic(
        sizeof(usbRxStreamBuf),
        1u,   /* trigger level: wake receiver on any single byte */
        usbRxStreamBuf,
        &usbRxStreamStorage);
    assert(NULL != usbRxStream);
}

/**
 * @brief Append one byte to the TX accumulation buffer.
 *
 * Call for each byte of the response frame before calling
 * @c portserial_usb_flush_tx().  Silently drops bytes that would overflow the
 * buffer — this should never occur for valid Modbus frames (max 256 bytes).
 *
 * @param b  Byte to append.
 */
void portserial_usb_put_byte(uint8_t const b)
{
    if (usbTxLen < (uint16_t)USB_TX_BUF_SIZE)
    {
        usbTxBuf[usbTxLen++] = b;
    }
}

/**
 * @brief Flush the TX accumulation buffer via CDC_Transmit_FS.
 *
 * Sends the accumulated response frame as a single USB CDC transfer, then
 * resets the buffer length to zero for the next frame.
 *
 * @note CDC_Transmit_FS returns USBD_BUSY if a previous transfer has not
 *       completed.  At Modbus data rates this should never happen, but the
 *       return value is cast to void — if needed, add retry logic here.
 *       @todo [2026-04-25] Add optional vTaskDelay(pdMS_TO_TICKS(2u)) pre-delay
 *             if strict host applications require a t3.5 silence before the
 *             response.
 */
void portserial_usb_flush_tx(void)
{
    if (usbTxLen > 0u)
    {
        (void)CDC_Transmit_FS(usbTxBuf, usbTxLen);
        usbTxLen = 0u;
    }
}
