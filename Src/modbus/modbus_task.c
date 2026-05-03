/**
 * @file modbus_task.c
 * @brief FreeRTOS task and freemodbus register callbacks for Modbus RTU slave.
 *
 * Implements the FreeRTOS task that drives the freemodbus polling loop and the
 * four mandatory register-access callbacks required by the freemodbus stack
 * (input, holding, coil, discrete).  Register storage is owned by @c modbus_mem;
 * all callbacks acquire the shared mutex before iterating over registers and
 * release it immediately after.  Coils and discrete inputs are not supported
 * and always return MB_ENOREG.
 */

#include "modbus_task.h"
#include "modbus_mem.h"
#include "port_addresses.h"
#include "mb.h"

#include <assert.h>
#include <stddef.h>

#include "stm32l5xx_hal.h"
#include "system_task.h"

#include "FreeRTOS.h"
#include "task.h"

/* UART Modbus default configuration */
#ifndef DEFAULT_MODE
#define DEFAULT_MODE        MB_RTU
#endif

#ifndef DEFAULT_BAUDERATE
#define DEFAULT_BAUDERATE   115200
#endif

#ifndef DEFAULT_PARITY
#define DEFAULT_PARITY      MB_PAR_NONE
#endif

#ifndef DEFAULT_STOP_BITS
#define DEFAULT_STOP_BITS   1u
#endif
/**
 * @brief FreeRTOS task entry point for the Modbus RTU slave.
 *
 * Initialises the shared register memory via @c mb_mem_init(), then configures
 * and enables the freemodbus stack (RTU, slave address @c DEFAULT_SLAVE_ADDR,
 * 115200 8N1).  Enters the infinite polling loop that drives the stack state
 * machine.
 *
 * @param pvParameters  Unused FreeRTOS task parameter (pass NULL).
 */
void modbus_task(void * pvParameters)
{
    (void)pvParameters;

    mb_mem_init();

    // todo [2026-04-18] Check if flash has a setting saved for slave address / baud
    (void)eMBInit(DEFAULT_MODE,
                  DEFAULT_SLAVE_ADDR,
                  0,                   /* port — not used by this BSP */
                  DEFAULT_BAUDERATE,
                  DEFAULT_PARITY,
                  DEFAULT_STOP_BITS);

    (void)eMBEnable();

    while (1)
    {
        (void)eMBPoll();
        system_task_check_in(SYSTEM_TASK_ID_MODBUS);
    }
}

/**
 * @brief freemodbus callback — read input registers (function code 0x04).
 *
 * Acquires the shared register mutex for the duration of the read, then copies
 * the requested register values from modbus_mem into @p pRegBuffer in
 * big-endian byte order.
 *
 * @param pRegBuffer  Destination buffer managed by the freemodbus stack.
 * @param address     Starting Modbus register address.
 * @param nRegs       Number of registers requested.
 * @return MB_ENOERR  Requested range read successfully.
 * @return MB_ENOREG  Range out of bounds or mutex unavailable.
 */
eMBErrorCode eMBRegInputCB(UCHAR * pRegBuffer, USHORT address, USHORT nRegs)
{
    if ((address < (USHORT)REG_INPUT_START) ||
        ((USHORT)(address + nRegs) > (USHORT)(REG_INPUT_START + REG_INPUT_NREGS)))
    {
        return MB_ENOREG;
    }

    if (!mb_mem_get_mutex())
    {
        return MB_ENOREG;
    }

    for (USHORT i = 0u; i < nRegs; i++)
    {
        uint16_t const * pReg = mb_mem_get_input((uint16_t)(address + i));
        uint16_t const   val  = (pReg != NULL) ? *pReg : 0u;
        *pRegBuffer++ = (UCHAR)(val >> 8u);
        *pRegBuffer++ = (UCHAR)(val & 0xFFu);
    }

    mb_mem_release_mutex();
    return MB_ENOERR;
}

/**
 * @brief freemodbus callback — read/write holding registers (function codes 0x03 / 0x10).
 *
 * Acquires the shared register mutex for the duration of the operation.  On a
 * read (@c MB_REG_READ) register values are copied into @p pRegBuffer in
 * big-endian order.  On a write (@c MB_REG_WRITE) big-endian bytes from
 * @p pRegBuffer are assembled into host-order values and stored via
 * @c mb_mem_set_holding().
 *
 * @param pRegBuffer  Source or destination buffer managed by the freemodbus stack.
 * @param address     Starting Modbus register address.
 * @param nRegs       Number of registers to read or write.
 * @param eMode       @c MB_REG_READ or @c MB_REG_WRITE.
 * @return MB_ENOERR  Operation completed successfully.
 * @return MB_ENOREG  Range out of bounds, mutex unavailable, or write failed.
 */
eMBErrorCode
eMBRegHoldingCB(UCHAR * pRegBuffer, USHORT address, USHORT nRegs,
                              eMBRegisterMode eMode)
{
    if ((address < (USHORT)REG_HOLDING_START) ||
        ((USHORT)(address + nRegs) > (USHORT)(REG_HOLDING_START + REG_HOLDING_NREGS)))
    {
        return MB_ENOREG;
    }

    if (!mb_mem_get_mutex())
    {
        return MB_ENOREG;
    }

    eMBErrorCode eStatus = MB_ENOERR;

    switch (eMode)
    {
    case MB_REG_READ:
        for (USHORT i = 0u; i < nRegs; i++)
        {
            uint16_t const * pReg = mb_mem_get_holding((uint16_t)(address + i));
            uint16_t const   val  = (pReg != NULL) ? *pReg : 0u;
            *pRegBuffer++ = (UCHAR)(val >> 8u);
            *pRegBuffer++ = (UCHAR)(val & 0xFFu);
        }
        break;

    case MB_REG_WRITE:
    {
        /* Assemble host-order values from big-endian PDU bytes, then write
         * the entire range in one call so the mutex covers the full batch. */
        uint16_t tmpBuf[REG_HOLDING_NREGS];
        for (USHORT i = 0u; i < nRegs; i++)
        {
            tmpBuf[i]  = (uint16_t)((uint16_t)(*pRegBuffer++) << 8u);
            tmpBuf[i] |= (uint16_t)(*pRegBuffer++);
        }
        if (!mb_mem_set_holding((uint16_t)address, tmpBuf, (size_t)nRegs))
        {
            eStatus = MB_ENOREG;
        }
        break;
    }

    default:
        break;
    }

    mb_mem_release_mutex();
    return eStatus;
}

/**
 * @brief freemodbus callback — read/write coil registers (function codes 0x01 / 0x0F).
 *
 * Coil registers are not implemented on this device.
 *
 * @return MB_ENOREG  always.
 */
eMBErrorCode eMBRegCoilsCB(UCHAR * pRegBuffer, USHORT address, USHORT usNCoils,
                            eMBRegisterMode eMode)
{
    (void)pRegBuffer;
    (void)address;
    (void)usNCoils;
    (void)eMode;
    return MB_ENOREG;
}

/**
 * @brief freemodbus callback — read discrete input registers (function code 0x02).
 *
 * Discrete inputs are not implemented on this device.
 *
 * @return MB_ENOREG  always.
 */
eMBErrorCode eMBRegDiscreteCB(UCHAR * pRegBuffer, USHORT address, USHORT usNDiscrete)
{
    (void)pRegBuffer;
    (void)address;
    (void)usNDiscrete;
    return MB_ENOREG;
}
