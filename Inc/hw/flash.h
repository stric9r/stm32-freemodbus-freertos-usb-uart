/**
  ******************************************************************************
  * @file           : flash.h
  * @brief          : STM32L5 flash erase, write, and read API
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void flash_init(void);
bool flash_erase(uint32_t bank, uint32_t page, uint32_t nb_pages);
bool flash_write(uint32_t addr, void const * const p_data, size_t len);
bool flash_read(uint32_t addr, void * const p_buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_H */
