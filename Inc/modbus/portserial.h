/**
  ******************************************************************************
  * @file           : portserial.h
  * @brief          : Transport mode selection (COMMS_MODBUS_PORT)
  *                   and UART defaults
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef PORTSERIAL_H
#define PORTSERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Default Modbus port configuration — change these to remap compile-time defaults */
#ifndef DEFAULT_SLAVE_ADDR
#define DEFAULT_SLAVE_ADDR  0x0Au
#endif

#ifndef DEFAULT_MODE
#define DEFAULT_MODE        MB_RTU
#endif

#ifndef DEFAULT_BAUDERATE
#define DEFAULT_BAUDERATE   115200UL
#endif

#ifndef DEFAULT_PARITY
#define DEFAULT_PARITY      MB_PAR_NONE
#endif

#ifndef DEFAULT_STOP_BITS
#define DEFAULT_STOP_BITS   1u
#endif

/* Select active transport — set for your product */
#define COMMS_MODBUS_UART     0
#define COMMS_MODBUS_USB      1
#define COMMS_MODBUS_DYNAMIC  2

#ifndef COMMS_MODBUS_PORT
#define COMMS_MODBUS_PORT     COMMS_MODBUS_DYNAMIC
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORTSERIAL_H */
