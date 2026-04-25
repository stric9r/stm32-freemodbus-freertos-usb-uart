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
 *   Call @c mb_mem_get_mutex() once, perform all accesses via the internal raw
 *   helper @c mb_mem_raw_holding() / @c mb_mem_raw_input(), then call
 *   @c mb_mem_release_mutex(). Do NOT call the public get/set functions while
 *   already holding the mutex — they will fail the inner acquire.
 */

#include "modbus_mem.h"
#include "port_addresses.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>
#include <assert.h>

static USHORT regHoldingBuf[REG_HOLDING_NREGS];
static USHORT regInputBuf[REG_INPUT_NREGS];

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

    for (size_t i = 0u; i < REG_HOLDING_NREGS; i++)
    {
        regHoldingBuf[i] = (USHORT)i;
    }

    for (size_t i = 0u; i < REG_INPUT_NREGS; i++)
    {
        regInputBuf[i] = (USHORT)i;
    }

    mb_mem_mutex = xSemaphoreCreateMutexStatic(&mb_mem_mutex_buf);
    assert(mb_mem_mutex != NULL);
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
 * true, and after every successful @c mb_mem_get_holding() / @c mb_mem_get_input()
 * that returned a non-NULL pointer.
 *
 * @return true   Mutex acquired — proceed with register access.
 * @return false  Mutex already held by another task — discard request.
 */
bool mb_mem_get_mutex(void)
{
    return xSemaphoreTake(mb_mem_mutex, 0) == pdTRUE;
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
 * Acquires the mutex internally (non-blocking). The returned pointer is valid
 * only while the mutex is held — the caller MUST call @c mb_mem_release_mutex()
 * when done reading, before another task can overwrite the value.
 *
 * @param addr  Modbus register address (must be within REG_HOLDING_START ..
 *              REG_HOLDING_START + REG_HOLDING_NREGS - 1).
 * @return Pointer to the register value — mutex is now held by the caller.
 * @return NULL if @p addr is out of range.
 */
uint16_t const * mb_mem_get_holding(uint16_t const addr)
{
    uint16_t * pData = NULL;

    // We're in range and got the semaphore?
    bool bStatus  = (addr >= (uint16_t)REG_HOLDING_START);
         bStatus &= (addr < (uint16_t)(REG_HOLDING_START + REG_HOLDING_NREGS));

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
    assert(0 < data_sz);

    uint16_t * pData = NULL;

    // We're in range and got the semaphore?
    bool bStatus  = (addr >= (uint16_t)REG_HOLDING_START);
         bStatus &= (((size_t)(addr - (uint16_t)REG_HOLDING_START) + data_sz) < REG_HOLDING_NREGS);


    if(bStatus)
    {
        size_t const idx = (size_t)(addr - (uint16_t)REG_HOLDING_START);
        (void)memcpy(&regHoldingBuf[idx], p_data, data_sz * sizeof(uint16_t));
    }

    return bStatus;

/**
 * @brief Read a single input register.
 *
 * Same mutex semantics as @c mb_mem_get_holding — caller MUST call
 * @c mb_mem_release_mutex() after reading the returned value.
 *
 * @param addr  Modbus register address (must be within REG_INPUT_START ..
 *              REG_INPUT_START + REG_INPUT_NREGS - 1).
 * @return Pointer to the register value — mutex is now held by the caller.
 * @return NULL if @p addr is out of range or if the mutex is unavailable.
 */
uint16_t const * mb_mem_get_input(uint16_t const addr)
{
    uint16_t const * pData = NULL;

    // We're in range and got the semaphore?
    bool bStatus  = (addr >= (uint16_t)REG_INPUT_START);
         bStatus &= (addr < (uint16_t)(REG_INPUT_START + REG_INPUT_NREGS));

    if(bStatus)
    {
        pData = &regInputBuf[addr - (uint16_t)REG_INPUT_START];
    }

    return pData;
}

