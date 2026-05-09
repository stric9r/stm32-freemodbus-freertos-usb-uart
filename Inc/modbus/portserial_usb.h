/**
  ******************************************************************************
  * @file           : portserial_usb.h
  * @brief          : USB CDC serial port layer: RX stream buffer,
  *                   TX flush, transport-mux API
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef PORTSERIAL_USB_H
#define PORTSERIAL_USB_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "stream_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RX stream buffer — written by CDC_Receive_FS ISR, read by usb_task */
extern StreamBufferHandle_t usbRxStream;

/* Low-level USB CDC byte layer (portserial_usb.c) */
void portserial_usb_init(void);
void portserial_usb_put_byte(uint8_t const b);
void portserial_usb_flush_tx(void);

/*
 * Transport-mux API (implemented in portserial.c — these functions modify
 * state that lives alongside the UART serial port functions so that the
 * FreeModbus port layer can route bytes to/from USB vs UART transparently).
 *
 * vMBPortSetUsbActive()   — call before injecting a USB frame; disables UART
 *                           byte routing and redirects FreeModbus I/O to USB.
 *                           Automatically cleared after the response TX cycle
 *                           completes inside vMBPortSerialEnable(TRUE,FALSE).
 *
 * vMBPortUsbInjectFrame() — feed a validated USB CDC frame byte-by-byte
 *                           through xMBRTUReceiveFSM(), then fire a synthetic
 *                           t3.5 expiry to trigger EV_FRAME_RECEIVED so
 *                           eMBPoll() processes the frame as if it came from
 *                           UART.
 */
void vMBPortSetUsbActive(bool active);
void vMBPortUsbInjectFrame(uint8_t const * pFrame, uint16_t len);

/*
 * xMBPortIsUsbActive() — read by porttimer.c to suppress LPTIM1 during injection.
 * Declared here (not as a raw extern in porttimer.c) to keep module coupling
 * explicit through header includes.
 */
bool xMBPortIsUsbActive(void);

#ifdef __cplusplus
}
#endif

#endif /* PORTSERIAL_USB_H */
