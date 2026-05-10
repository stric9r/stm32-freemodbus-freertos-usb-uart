/**
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
/**
 * @file modbus_task.c
 * @brief FreeRTOS task and freemodbus register callbacks for Modbus RTU slave.
 *
 * Implements the FreeRTOS task that drives the freemodbus polling loop and the
 * four mandatory register-access callbacks required by the freemodbus stack
 * (input, holding, coil, discrete).  Register storage is owned by @c modbus_mem;
 * all callbacks acquire the shared mutex before iterating over registers and
 * release it immediately after.  Coils drive PE0-PE7 directly; discrete inputs
 * report GREEN/RED/BLUE LED drive state and the BUTTON pin.
 */

#include "modbus_task.h"
#include "modbus_mem.h"
#include "port_addresses.h"
#include "mb.h"
#include "port_internal.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "stm32l5xx_hal.h"
#include "system_task.h"

#include "FreeRTOS.h"
#include "task.h"

/* PE0-PE7 in coil-address order (coil 1 = PE0, coil 8 = PE7) */
static const uint16_t coilPins[GPIO_COIL_NPINS] = {
    GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3,
    GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7,
};

/* Discrete layout — must stay consistent with REG_DISCRETE_NGPIO (4).
 * Discrete 1: GREEN LED (PC7, ODR via IDR on push-pull)
 * Discrete 2: RED LED   (PA9, ODR via IDR on push-pull)
 * Discrete 3: BLUE LED  (PB7, ODR via IDR on push-pull)
 * Discrete 4: BUTTON    (PC13, IDR)
 * Discretes 5-16: always 0 (no hardware backing). */
static GPIO_TypeDef * const discretePorts[REG_DISCRETE_NGPIO] = {
    GPIO_GREEN_LED_GPIO_Port,
    GPIO_RED_LED_GPIO_Port,
    GPIO_BLUE_LED_GPIO_Port,
    GPIO_BUTTON_GPIO_Port,
};
static const uint16_t discretePins[REG_DISCRETE_NGPIO] = {
    GPIO_GREEN_LED_Pin,
    GPIO_RED_LED_Pin,
    GPIO_BLUE_LED_Pin,
    GPIO_BUTTON_Pin,
};

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

    modbus_cfg_t const * const pCfg = mb_mem_get_config();
    (void)eMBInit((eMBMode)pCfg->mode,
                  pCfg->slaveAddr,
                  0,                   /* port — not used by this BSP */
                  pCfg->baudRate,
                  (eMBParity)pCfg->parity,
                  pCfg->stopBits);

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

    for (USHORT i = 0u; i < nRegs; i++)
    {
        uint16_t const * pReg = mb_mem_get_input((uint16_t)(address + i));
        uint16_t const   val  = (pReg != NULL) ? *pReg : 0u;
        *pRegBuffer++ = (UCHAR)(val >> 8u);
        *pRegBuffer++ = (UCHAR)(val & 0xFFu);
    }

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
    
    return eStatus;
}

/**
 * @brief freemodbus callback — read/write coil registers (function codes 0x01 / 0x05 / 0x0F).
 *
 * Coils map to PE0-PE7. Bits are LSB-first within each byte of @p pRegBuffer,
 * matching the Modbus wire format. Reads reflect the current ODR state (via IDR
 * on the push-pull pins); writes drive the GPIO directly.
 *
 * @param pRegBuffer  Bit-packed source or destination buffer.
 * @param address     Starting Modbus coil address.
 * @param usNCoils    Number of coils to read or write.
 * @param eMode       @c MB_REG_READ or @c MB_REG_WRITE.
 * @return MB_ENOERR  Operation completed successfully.
 * @return MB_ENOREG  Address range out of bounds.
 */
eMBErrorCode eMBRegCoilsCB(UCHAR * pRegBuffer, USHORT address, USHORT usNCoils,
                            eMBRegisterMode eMode)
{
    bool bInRange  = (address >= (USHORT)REG_COIL_START);
         bInRange &= ((USHORT)(address - (USHORT)REG_COIL_START + usNCoils)
                      <= (USHORT)REG_COIL_NREGS);

    if (!bInRange)
    {
        return MB_ENOREG;
    }

    USHORT const offset = address - (USHORT)REG_COIL_START;

    if (MB_REG_WRITE == eMode)
    {
        for (USHORT i = 0u; i < usNCoils; i++)
        {
            /* Modbus packs bits LSB-first, 8 per byte: bit i is at byte i/8,
             * bit position i%8. Shift it down to position 0 then mask to 0 or 1. */
            uint8_t const byte = i / 8u;
            uint8_t const bit = i % 8u;

             GPIO_PinState const state =
                ((pRegBuffer[byte] >> bit) & 1u)
                ? GPIO_PIN_SET : GPIO_PIN_RESET;
            HAL_GPIO_WritePin(GPIO_COIL_PORT, coilPins[offset + i], state);
        }
    }
    else
    {
        (void)memset(pRegBuffer, 0, ((size_t)usNCoils + 7u) / 8u);
        for (USHORT i = 0u; i < usNCoils; i++)
        {
            if (GPIO_PIN_SET == HAL_GPIO_ReadPin(GPIO_COIL_PORT,
                                                 coilPins[offset + i]))
            {
                /* Shift a 1 into bit position i%8 of byte i/8 to set that coil's bit. */
                uint8_t const byte = i / 8u;
                uint8_t const bit = i % 8u;
                pRegBuffer[byte] |= (UCHAR)(1u << bit);
            }
        }
    }

    return MB_ENOERR;
}

/**
 * @brief freemodbus callback — read discrete inputs (function code 0x02).
 *
 * Reports LED drive state (via IDR — push-pull pins track ODR) and BUTTON
 * pin level. Bits are LSB-first within each byte of @p pRegBuffer. The
 * BUTTON discrete is active-high at the IDR level; the physical button is
 * active-low, so the bit is 1 when the button is not pressed.
 *
 * @param pRegBuffer   Bit-packed destination buffer.
 * @param address      Starting Modbus discrete address.
 * @param usNDiscrete  Number of discrete inputs to read.
 * @return MB_ENOERR   Read completed successfully.
 * @return MB_ENOREG   Address range out of bounds.
 */
eMBErrorCode eMBRegDiscreteCB(UCHAR * pRegBuffer, USHORT address,
                               USHORT usNDiscrete)
{
    bool bInRange  = (address >= (USHORT)REG_DISCRETE_START);
         bInRange &= ((USHORT)(address - (USHORT)REG_DISCRETE_START + usNDiscrete)
                      <= (USHORT)REG_DISCRETE_NREGS);

    if (!bInRange)
    {
        return MB_ENOREG;
    }

    USHORT const offset = address - (USHORT)REG_DISCRETE_START;

    (void)memset(pRegBuffer, 0, ((size_t)usNDiscrete + 7u) / 8u);

    for (USHORT i = 0u; i < usNDiscrete; i++)
    {
        USHORT const idx = offset + i;
        /* Indices beyond REG_DISCRETE_NGPIO have no hardware; leave bit 0. */
        if ((idx < (USHORT)REG_DISCRETE_NGPIO) &&
            (GPIO_PIN_SET == HAL_GPIO_ReadPin(discretePorts[idx],
                                              discretePins[idx])))
        {
            /* Shift a 1 into bit position i%8 of byte i/8 to set that discrete's bit. */
            uint8_t const byte = i / 8u;
            uint8_t const bit  = i % 8u;
            pRegBuffer[byte] |= (UCHAR)(1u << bit);
        }
    }

    return MB_ENOERR;
}
