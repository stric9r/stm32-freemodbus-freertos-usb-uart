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

void             mb_mem_init(void);
bool             mb_mem_get_mutex(void);
void             mb_mem_release_mutex(void);
uint16_t const * mb_mem_get_holding(uint16_t const addr);
bool             mb_mem_set_holding(uint16_t const addr,
                                    uint16_t const * const p_data,
                                    size_t   const data_sz);
uint16_t const * mb_mem_get_input(uint16_t const addr);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MEM_H */
