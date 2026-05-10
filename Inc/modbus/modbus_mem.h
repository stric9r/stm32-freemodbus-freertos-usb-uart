/**
  ******************************************************************************
  * @file           : modbus_mem.h
  * @brief          : Shared Modbus register bank API
  *                   (holding and input registers)
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef MODBUS_MEM_H
#define MODBUS_MEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Modbus port configuration stored in FLASH_MB.
 * CRC covers all bytes before the crc field (offsetof(modbus_cfg_t, crc) = 12).
 * Add fields here to persist additional settings; expand FLASH_MB LENGTH accordingly. */
typedef struct {
    uint8_t  slaveAddr;
    uint8_t  mode;         /* eMBMode cast to uint8_t   */
    uint8_t  _pad[2];      /* explicit pad to 4-byte-align baudRate */
    uint32_t baudRate;
    uint8_t  parity;       /* eMBParity cast to uint8_t */
    uint8_t  dataBits;     /* informational — FreeModbus RTU hardcodes 8 internally */
    uint8_t  stopBits;
    uint8_t  _pad2;        /* explicit alignment pad */
    uint16_t crc;          /* CRC-16/Modbus over bytes [0, offsetof(crc)) */
    uint8_t  _reserved[2]; /* pad to 16 bytes (2 doublewords) for HAL flash writes */
} modbus_cfg_t;

void             mb_mem_init(void);
uint16_t const * mb_mem_get_holding(uint16_t const addr);
bool             mb_mem_set_holding(uint16_t const addr,
                                    uint16_t const * const p_data,
                                    size_t   const data_sz);
uint16_t const * mb_mem_get_input(uint16_t const addr);
volatile modbus_cfg_t const * mb_mem_get_config(void);
bool                 mb_mem_set_config(modbus_cfg_t const * const p_cfg);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MEM_H */
