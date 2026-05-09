/**
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
/**
 * @file modbus_mem.c
 * @brief Modbus register memory with mutex-protected access.
 *
 * Owns the holding and input register arrays for the Modbus slave. All access
 * from both the UART Modbus stack and the USB Modbus handler must go through
 * this module. The internal mutex (@c mb_mem_mutex) is never exported — callers
 * use @c mb_mem_get_mutex() and @c mb_mem_release_mutex() for explicit batch
 * control, and the individual get/set functions for single-operation access.
 *
 * Mutex strategy — fail-fast, non-blocking:
 *   All mutex acquisitions use timeout=0. If the mutex is unavailable, functions
 *   return immediately with an error indicator (NULL or false). Callers must not
 *   spin or retry — they should discard the request and let the Modbus master
 *   retry on the next poll cycle.
 *
 * Batch operations (FreeModbus callbacks iterate over N registers):
 *   Call @c mb_mem_get_mutex() once, perform all accesses via the public
 *   get/set functions (which do not re-acquire the mutex), then call
 *   @c mb_mem_release_mutex().
 */

#include "modbus_mem.h"
#include "port_addresses.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>
#include <assert.h>

static uint16_t regHoldingBuf[REG_HOLDING_NREGS];
static uint16_t regInputBuf[REG_INPUT_NREGS];

static StaticSemaphore_t mb_mem_mutex_buf;
static SemaphoreHandle_t mb_mem_mutex;

/**
 * @brief Initialise the register memory space and create the access mutex.
 *
 * Pre-fills each register with its own zero-based index value so contents are
 * deterministic before any master writes them. Must be called once before any
 * other @c mb_mem_* function.
 */
void mb_mem_init(void)
{
    // @todo [2026-04-25] Read initial holding register values from flash on startup.
    // @todo [2026-04-25] Persist holding register writes to flash on change.

    for (size_t idx = 0u; idx < REG_HOLDING_NREGS; idx++)
    {
        regHoldingBuf[idx] = (uint16_t)idx;
    }

    for (size_t idx = 0u; idx< REG_INPUT_NREGS; idx++)
    {
        regInputBuf[idx] = (uint16_t)idx;
    }

    mb_mem_mutex = xSemaphoreCreateMutexStatic(&mb_mem_mutex_buf);
    assert(NULL != mb_mem_mutex);
}

/**
 * @brief Attempt to acquire the register mutex (non-blocking, fail-fast).
 *
 * Use this before a batch of register accesses that must be atomic across
 * multiple registers. Individual @c mb_mem_get_holding() / @c mb_mem_set_holding()
 * calls also acquire the mutex internally — use this function only when you
 * need explicit batch control.
 *
 * Callers MUST call @c mb_mem_release_mutex() after every successful return of
 * true to release the mutex when the batch operation is complete.
 *
 * @return true   Mutex acquired — proceed with register access.
 * @return false  Mutex already held by another task — discard request.
 */
bool mb_mem_get_mutex(void)
{
    return (pdTRUE == xSemaphoreTake(mb_mem_mutex, 0));
}

/**
 * @brief Release the register mutex.
 *
 * Must be called after every successful @c mb_mem_get_mutex() and after every
 * successful @c mb_mem_get_holding() / @c mb_mem_get_input() call that returned
 * a non-NULL pointer.
 */
void mb_mem_release_mutex(void)
{
    (void)xSemaphoreGive(mb_mem_mutex);
}

/**
 * @brief Read a single holding register.
 *
 * Caller must hold the mutex via @c mb_mem_get_mutex() before calling this
 * function, and release it with @c mb_mem_release_mutex() when done.
 *
 * @param addr  Modbus register address (must be within REG_HOLDING_START ..
 *              REG_HOLDING_START + REG_HOLDING_NREGS - 1).
 * @return Pointer to the register value.
 * @return NULL if @p addr is out of range.
 */
uint16_t const * mb_mem_get_holding(uint16_t const addr)
{
    uint16_t * pData = NULL;

    bool bStatus  = (addr >= (uint16_t)REG_HOLDING_START);
         bStatus &= (addr <  (uint16_t)(REG_HOLDING_START + REG_HOLDING_NREGS));

    if(bStatus)
    {
        pData = &regHoldingBuf[addr - (uint16_t)REG_HOLDING_START];
    }

    return pData;
}

/**
 * @brief Write one or more consecutive holding registers.
 *
 * Acquires and releases the mutex internally (non-blocking, fail-fast).
 * Copies @p data_sz register values from @p p_data into the buffer starting
 * at @p addr.
 *
 * @param addr     Starting Modbus register address.
 * @param p_data   Pointer to source data (host byte order).
 * @param data_sz  Number of registers to write.
 * @return true   All registers written successfully.
 * @return false  @p addr out of range, or range overflows.
 */
bool mb_mem_set_holding(uint16_t const addr,
                        uint16_t const * const p_data,
                        size_t   const data_sz)
{
    assert(NULL != p_data);
    assert(0u < data_sz);

    bool bStatus  = (addr >= (uint16_t)REG_HOLDING_START);
         bStatus &= (((size_t)(addr - (uint16_t)REG_HOLDING_START) + data_sz) <= REG_HOLDING_NREGS);

    if(bStatus)
    {
        size_t const idx = (size_t)(addr - (uint16_t)REG_HOLDING_START);
        (void)memcpy(&regHoldingBuf[idx], p_data, data_sz * sizeof(uint16_t));
    }

    return bStatus;
}

/**
 * @brief Read a single input register.
 *
 * Caller must hold the mutex via @c mb_mem_get_mutex() before calling this
 * function, and release it with @c mb_mem_release_mutex() when done.
 *
 * @param addr  Modbus register address (must be within REG_INPUT_START ..
 *              REG_INPUT_START + REG_INPUT_NREGS - 1).
 * @return Pointer to the register value.
 * @return NULL if @p addr is out of range.
 */
uint16_t const * mb_mem_get_input(uint16_t const addr)
{
    uint16_t const * pData = NULL;

    bool bStatus  = (addr >= (uint16_t)REG_INPUT_START);
         bStatus &= (addr <  (uint16_t)(REG_INPUT_START + REG_INPUT_NREGS));

    if(bStatus)
    {
        pData = &regInputBuf[addr - (uint16_t)REG_INPUT_START];
    }

    return pData;
}

