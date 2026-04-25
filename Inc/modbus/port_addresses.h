#ifndef PORT_ADDRESSES_H
#define PORT_ADDRESSES_H

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

#endif /* PORT_ADDRESSES_H */
