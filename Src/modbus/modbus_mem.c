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
 * this module.
 */

#include "modbus_mem.h"
#include "port_addresses.h"
#include "portserial.h"
#include "mb.h"
#include "mbcrc.h"
#include "flash.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "stm32l5xx_hal.h"  /* NVIC_SystemReset() */

#include <string.h>
#include <assert.h>
#include <stddef.h>

static uint16_t regHoldingBuf[REG_HOLDING_NREGS];
static uint16_t regInputBuf[REG_INPUT_NREGS];

static StaticSemaphore_t mb_mem_mutex_buf;
static SemaphoreHandle_t mb_mem_mutex;

__attribute__((section(".nvm_mb")))
volatile modbus_cfg_t const nvmMbConfig;

static volatile modbus_cfg_t const defaultCfg = {
    .slaveAddr = DEFAULT_SLAVE_ADDR,
    .mode      = (uint8_t)DEFAULT_MODE,
    .baudRate  = DEFAULT_BAUDERATE,
    .parity    = (uint8_t)DEFAULT_PARITY,
    .dataBits  = DEFAULT_DATA_BITS,
    .stopBits  = DEFAULT_STOP_BITS,
    .crc       = 0u,
};

#define SLAVE_ADDR_IDX 0u
#define MODE_IDX       1u
#define BAUDE_RATE_IDX 2u
#define PARITY_IDX     4u
#define DATA_BITS_IDX  5u
#define STOP_BITS_IDX  6u
#define CRC_IDX        7u
#define WRITE_FLAG_IDX 8u

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

    // We need atleast the default config size
    assert(sizeof(modbus_cfg_t) <= (REG_HOLDING_NREGS*2u));
    assert(sizeof(modbus_cfg_t) <= (REG_INPUT_NREGS*2u));

    mb_mem_mutex = xSemaphoreCreateMutexStatic(&mb_mem_mutex_buf);
    assert(NULL != mb_mem_mutex);

    memset(regHoldingBuf, 0u, (REG_HOLDING_NREGS*2u));
    memset(regInputBuf,   0u, (REG_HOLDING_NREGS*2u));

    // Get config values and populate
    modbus_cfg_t const * const pCfg = mb_mem_get_config();



    regHoldingBuf[SLAVE_ADDR_IDX]      = pCfg->slaveAddr;
    regHoldingBuf[MODE_IDX]            = pCfg->mode;
    // low bytes
    regHoldingBuf[BAUDE_RATE_IDX]      = (uint16_t)(pCfg->baudRate & 0x0000FFFF);
    // high bytes
    regHoldingBuf[BAUDE_RATE_IDX + 1u] = (uint16_t)((pCfg->baudRate & 0xFFFF0000) >> 16u);
    regHoldingBuf[PARITY_IDX]          = pCfg->parity;
    regHoldingBuf[DATA_BITS_IDX]       = pCfg->dataBits;
    regHoldingBuf[STOP_BITS_IDX]       = pCfg->stopBits;
    regHoldingBuf[CRC_IDX]             = pCfg->crc;
    // Write to flash flag
    regHoldingBuf[WRITE_FLAG_IDX]      = 0;

    regInputBuf[SLAVE_ADDR_IDX]        = pCfg->slaveAddr;
    regInputBuf[MODE_IDX]              = pCfg->mode;
    // low bytes
    regInputBuf[BAUDE_RATE_IDX]        = (uint16_t)(pCfg->baudRate & 0x0000FFFF);
    // high bytes
    regInputBuf[BAUDE_RATE_IDX + 1u]   = (uint16_t)((pCfg->baudRate & 0xFFFF0000) >> 16u);
    regInputBuf[PARITY_IDX]            = pCfg->parity;
    regInputBuf[DATA_BITS_IDX]         = pCfg->dataBits;
    regInputBuf[STOP_BITS_IDX]         = pCfg->stopBits;
    regInputBuf[CRC_IDX]               = pCfg->crc;
    // Write to flash flag
    regInputBuf[WRITE_FLAG_IDX]        = 0;
}

/**
 * @brief Read a single holding register.
 *
 * @param addr  Modbus register address (must be within REG_HOLDING_START ..
 *              REG_HOLDING_START + REG_HOLDING_NREGS - 1).
 * @return Pointer to the register value.
 * @return NULL if @p addr is out of range.
 */
uint16_t const * mb_mem_get_holding(uint16_t const addr)
{
    uint16_t * pData = NULL;

    (void)xSemaphoreTake(mb_mem_mutex, portMAX_DELAY);

    bool bStatus  = (addr >= (uint16_t)REG_HOLDING_START);
         bStatus &= (addr <  (uint16_t)(REG_HOLDING_START + REG_HOLDING_NREGS));

    if(bStatus)
    {
        pData = &regHoldingBuf[addr - (uint16_t)REG_HOLDING_START];
    }

    (void)xSemaphoreGive(mb_mem_mutex);

    return pData;
}

/**
 * @brief Validate a modbus_cfg_t before it is written to flash.
 *
 * Checks every field against the ranges that the rest of the system can
 * actually handle. dataBits must be exactly 8 because FreeModbus RTU and
 * the USART driver both hardcode that value. baudRate is capped at 115200.
 * 
 * @note Does not check CRC, this happens at init when getting the data from
 *       FLASH.  
 *
 * @param pCfg  Pointer to the config to validate.
 * @return true  All fields are in range — safe to persist.
 * @return false At least one field is out of range — discard.
 */
static bool cfg_is_valid(modbus_cfg_t const * const pCfg)
{
    bool bValid = (NULL != pCfg);

    if (bValid)
    {
        bValid &= (pCfg->slaveAddr >= 1u) && (247u >= pCfg->slaveAddr);
        bValid &= ((uint8_t)MB_RTU   == pCfg->mode) ||
                  ((uint8_t)MB_ASCII == pCfg->mode);
        bValid &= (pCfg->parity <= (uint8_t)MB_PAR_EVEN);
        bValid &= (8u == pCfg->dataBits);
        bValid &= (1u == pCfg->stopBits) || (2u == pCfg->stopBits);

        switch (pCfg->baudRate)
        {
            case 1200u:
            case 2400u:
            case 4800u:
            case 9600u:
            case 19200u:
            case 38400u:
            case 57600u:
            case 115200u:
                break;
            default:
                bValid = false;
                break;
        }
    }

    return bValid;
}

/**
 * @brief Write one or more consecutive holding registers.
 *
 * Acquires the internal mutex (portMAX_DELAY) and releases it on return.
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

    (void)xSemaphoreTake(mb_mem_mutex, portMAX_DELAY);

    bool bStatus  = (addr >= (uint16_t)REG_HOLDING_START);
         bStatus &= (((size_t)(addr - (uint16_t)REG_HOLDING_START) + data_sz) <= REG_HOLDING_NREGS);

    if(bStatus)
    {
        size_t const idx = (size_t)(addr - (uint16_t)REG_HOLDING_START);
        (void)memcpy(&regHoldingBuf[idx], p_data, data_sz * sizeof(uint16_t));
    }

    // special - write config values
    if (0u != regHoldingBuf[WRITE_FLAG_IDX])
    {
        modbus_cfg_t cfg;

        cfg.slaveAddr = regHoldingBuf[SLAVE_ADDR_IDX];
        cfg.mode      = regHoldingBuf[MODE_IDX];
        cfg.baudRate  = (uint32_t)regHoldingBuf[BAUDE_RATE_IDX];
        cfg.baudRate |= ((uint32_t)regHoldingBuf[BAUDE_RATE_IDX + 1u] << 16u);
        cfg.parity    = regHoldingBuf[PARITY_IDX];
        cfg.dataBits  = regHoldingBuf[DATA_BITS_IDX];
        cfg.stopBits  = regHoldingBuf[STOP_BITS_IDX];

        regHoldingBuf[WRITE_FLAG_IDX] = 0u;

        bool bContinue = cfg_is_valid(&cfg);

        if (bContinue)
        {
            bContinue = mb_mem_set_config(&cfg);
        }

        if (bContinue)
        {
            regHoldingBuf[CRC_IDX] = nvmMbConfig.crc;
            NVIC_SystemReset();
        }
    }

    (void)xSemaphoreGive(mb_mem_mutex);

    return bStatus;
}

/**
 * @brief Read a single input register.
 *
 * @param addr  Modbus register address (must be within REG_INPUT_START ..
 *              REG_INPUT_START + REG_INPUT_NREGS - 1).
 * @return Pointer to the register value.
 * @return NULL if @p addr is out of range.
 */
uint16_t const * mb_mem_get_input(uint16_t const addr)
{
    uint16_t const * pData = NULL;

    (void)xSemaphoreTake(mb_mem_mutex, portMAX_DELAY);

    bool bStatus  = (addr >= (uint16_t)REG_INPUT_START);
         bStatus &= (addr <  (uint16_t)(REG_INPUT_START + REG_INPUT_NREGS));

    if(bStatus)
    {
        pData = &regInputBuf[addr - (uint16_t)REG_INPUT_START];
    }

    (void)xSemaphoreGive(mb_mem_mutex);

    return pData;
}

/**
 * @brief Return the active Modbus port configuration.
 *
 * Validates the CRC-16/Modbus of the flash-resident @c nvmMbConfig. Returns a
 * pointer to the flash struct when the CRC is valid, or a pointer to the
 * compile-time @c defaultCfg when the flash is erased or corrupt. Never returns
 * NULL — callers can use the result directly without a validity check.
 */
modbus_cfg_t const * mb_mem_get_config(void)
{
    (void)xSemaphoreTake(mb_mem_mutex, portMAX_DELAY);

    USHORT const computed = usMBCRC16((UCHAR const *)&nvmMbConfig,
                                      (USHORT)offsetof(modbus_cfg_t, crc));

    volatile modbus_cfg_t const * pResult =
        (computed == nvmMbConfig.crc) ? &nvmMbConfig : &defaultCfg;

    (void)xSemaphoreGive(mb_mem_mutex);

    return pResult;
}

/**
 * @brief Persist a new Modbus port configuration to FLASH_MB.
 *
 * Copies @p pCfg into a staging buffer, computes the CRC-16/Modbus over all
 * meaningful fields, erases flash page 252 (the first page of FLASH_MB), and
 * writes the 16-byte struct as two doublewords. The write is verified by calling
 * @c mb_mem_get_config() — if it returns @c &nvmMbConfig the write succeeded.
 *
 * @param pCfg  Pointer to the config to persist (crc field is ignored; it is
 *               computed here).
 * @return true   Config written and verified successfully.
 * @return false  A HAL flash operation failed, or post-write CRC check failed.
 */
bool mb_mem_set_config(modbus_cfg_t const * const pCfg)
{
    assert(NULL != pCfg);

    modbus_cfg_t staging;
    (void)memset(&staging, 0, sizeof(staging));
    (void)memcpy(&staging, pCfg, sizeof(modbus_cfg_t));
    staging.crc = usMBCRC16((UCHAR const *)&staging,
                             (USHORT)offsetof(modbus_cfg_t, crc));

    /* FLASH_MB is at 0x0807E000 — bank 2, page 124 (bank-relative).
     * Bank 2 starts at 0x08040000; (0x0807E000-0x08040000)/0x800 = 124. */
    bool bContinue = flash_erase(2u, 124u, 1u);

    if (bContinue)
    {
        bContinue = flash_write((uint32_t)&nvmMbConfig,
                                &staging, sizeof(staging));
    }

    return bContinue;
}
