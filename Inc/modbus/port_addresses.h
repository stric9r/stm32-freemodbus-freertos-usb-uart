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

/* Holding registers — arbitrarily selected; set for product X */
#define REG_HOLDING_START   1u
#define REG_HOLDING_NREGS   100u

/* Input registers — arbitrarily selected; set for product X */
#define REG_INPUT_START     1u
#define REG_INPUT_NREGS     100u

/* Coil registers — PE0-PE7, one bit per pin */
#define REG_COIL_START      1u
#define REG_COIL_NREGS      8u

/* Discrete input registers — GREEN/RED/BLUE LED ODR + BUTTON IDR.
 * REG_DISCRETE_NREGS is declared as 16 so a master polling a full two-byte
 * block succeeds; bits REG_DISCRETE_NGPIO..15 always read 0.
 * REG_DISCRETE_NGPIO is the number of GPIO-backed entries in the discretePorts
 * and discretePins arrays in modbus_task.c — update both together. */
#define REG_DISCRETE_START  1u
#define REG_DISCRETE_NREGS  16u
#define REG_DISCRETE_NGPIO  4u

#ifdef __cplusplus
}
#endif

#endif /* PORT_ADDRESSES_H */
