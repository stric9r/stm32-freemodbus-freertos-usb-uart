/**
  ******************************************************************************
  * @file           : flash.c
  * @brief          : STM32L5 flash erase, write, and read driver
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
/**
 * @file flash.c
 * @brief Generic flash erase, write, and read for STM32L5.
 *
 * Wraps HAL_FLASH_Unlock/Lock, HAL_FLASHEx_Erase, and HAL_FLASH_Program
 * behind a clean API. An internal FreeRTOS mutex serialises all erase and
 * write operations so that only one caller can touch the flash controller at
 * a time. flash_read is a memory-mapped memcpy — the STM32L5 flash bus
 * allows data reads while no erase or program is in progress.
 *
 * HAL_FLASH_Lock is always called after an operation even on failure so the
 * flash controller is never left unlocked on an error return.
 */

#include "flash.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "stm32l5xx_hal.h"

#include <string.h>
#include <assert.h>

static StaticSemaphore_t flash_mutex_buf;
static SemaphoreHandle_t flash_mutex;

/**
 * @brief Create the internal flash mutex.
 *
 * Must be called once, before the scheduler starts, before any other
 * flash_* function.
 */
void flash_init(void)
{
    flash_mutex = xSemaphoreCreateMutexStatic(&flash_mutex_buf);
    assert(NULL != flash_mutex);
}

/**
 * @brief Erase one or more consecutive flash pages.
 *
 * @param bank      Flash bank number: 1 for bank 1, 2 for bank 2.
 *                  Any other value triggers an assert.
 * @param page      First page to erase, bank-relative (0-based).
 * @param nb_pages  Number of consecutive pages to erase.
 * @return true   Erase succeeded.
 * @return false  HAL erase returned an error.
 */
bool flash_erase(uint32_t const bank,
                 uint32_t const page,
                 uint32_t const nb_pages)
{
    assert(0u < nb_pages);
    assert((1u == bank) || (2u == bank));

    uint32_t const halBank = (1u == bank) ? FLASH_BANK_1 : FLASH_BANK_2;

    (void)xSemaphoreTake(flash_mutex, portMAX_DELAY);

    bool bContinue = (HAL_OK == HAL_FLASH_Unlock());

    if (bContinue)
    {
        FLASH_EraseInitTypeDef eraseInit = {
            .TypeErase = FLASH_TYPEERASE_PAGES,
            .Banks     = halBank,
            .Page      = page,
            .NbPages   = nb_pages,
        };
        uint32_t pageError = 0u;

        bContinue = (HAL_OK == HAL_FLASHEx_Erase(&eraseInit, &pageError));
    }

    (void)HAL_FLASH_Lock();
    (void)xSemaphoreGive(flash_mutex);

    return bContinue;
}

/**
 * @brief Write data to flash as consecutive doublewords.
 *
 * STM32L5 constraint: @p addr must be 8-byte aligned and @p len must be a
 * non-zero multiple of 8. Both are enforced by assert.
 *
 * @param addr    Destination flash address (8-byte aligned).
 * @param p_data  Source buffer (size >= @p len).
 * @param len     Byte count (non-zero, multiple of 8).
 * @return true   All doublewords written successfully.
 * @return false  A HAL program call returned an error.
 */
bool flash_write(uint32_t const addr,
                 void const * const p_data,
                 size_t const len)
{
    assert(NULL != p_data);
    assert(0u < len);
    assert(0u == (addr % 8u));
    assert(0u == (len  % 8u));

    (void)xSemaphoreTake(flash_mutex, portMAX_DELAY);

    bool bContinue = (HAL_OK == HAL_FLASH_Unlock());

    if (bContinue)
    {
        uint64_t const * const pWords = (uint64_t const *)p_data;
        size_t   const         nWords = len / 8u;

        for (size_t i = 0u; (i < nWords) && bContinue; i++)
        {
            bContinue = (HAL_OK == HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_DOUBLEWORD,
                addr + (i * 8u),
                pWords[i]));
        }
    }

    (void)HAL_FLASH_Lock();
    (void)xSemaphoreGive(flash_mutex);

    return bContinue;
}

/**
 * @brief Read bytes from memory-mapped flash into a RAM buffer.
 *
 * Flash is directly memory-mapped on STM32L5 — this is a plain memcpy
 * with no HAL call or mutex needed.
 *
 * @param addr   Source flash address.
 * @param p_buf  Destination RAM buffer (size >= @p len).
 * @param len    Byte count (non-zero).
 * @return true always.
 */
bool flash_read(uint32_t const addr, void * const p_buf, size_t const len)
{
    assert(NULL != p_buf);
    assert(0u < len);

    (void)memcpy(p_buf, (void const *)addr, len);

    return true;
}
