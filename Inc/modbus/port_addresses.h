/**
  ******************************************************************************
  * @file           : port_addresses.h
  * @brief          : Modbus register addresses, counts, and slave address
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef PORT_ADDRESSES_H
#define PORT_ADDRESSES_H

#include "mb.h"  /* eMBMode, eMBParity — needed for DEFAULT_MODE / DEFAULT_PARITY */

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

#ifndef DEFAULT_DATA_BITS
#define DEFAULT_DATA_BITS   8u
#endif

#ifndef DEFAULT_STOP_BITS
#define DEFAULT_STOP_BITS   1u
#endif

/* Holding registers — arbitrarily selected; set for product X */
#define REG_HOLDING_START   1u
#define REG_HOLDING_NREGS   100u

/* Input registers — arbitrarily selected; set for product X */
#define REG_INPUT_START     1u
#define REG_INPUT_NREGS     100u

/* Coil registers */
// @todo [2026-04-25] Not implemented — eMBRegCoilsCB returns MB_ENOREG
#define REG_COIL_START      0u
#define REG_COIL_NREGS      0u

/* Discrete input registers */
// @todo [2026-04-25] Not implemented — eMBRegDiscreteCB returns MB_ENOREG
#define REG_DISCRETE_START  0u
#define REG_DISCRETE_NREGS  0u

#ifdef __cplusplus
}
#endif

#endif /* PORT_ADDRESSES_H */
