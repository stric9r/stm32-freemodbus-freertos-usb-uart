/**
 * @file modbus_task.c
 * @brief FreeRTOS task and freemodbus register callbacks for Modbus RTU slave.
 *
 * Implements the FreeRTOS task that drives the freemodbus polling loop and the
 * four mandatory register-access callbacks required by the freemodbus stack
 * (input, holding, coil, discrete).  Holding and input register buffers are
 * statically allocated at compile time; coils and discrete inputs are not
 * supported and always return MB_ENOREG.
 */

#include "modbus_task.h"
#include "mb.h"

#include "assert.h"

#include "stm32l5xx_hal.h"
#include "watchdog.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stddef.h>


/* Modbus default modes*/
#define DEFAULT_MODE                MB_RTU
#define DEFAULT_SLAVE_ADDR          0x0A
#define DEFAULT_BAUDERATE           115200
#define DEFAULT_PARITY              MB_PAR_NONE
#define DEFAULT_STOP_BITS           1u


#define REG_HOLDING_START           0x1000
#define REG_HOLDING_NREGS           130
#define REG_INPUT_START             0x1000
#define REG_INPUT_NREGS             4

// Memory space
static USHORT   regHoldingStart = REG_HOLDING_START;
static USHORT   regHoldingBuf[REG_HOLDING_NREGS];
static USHORT   regInputStart = REG_INPUT_START;
static USHORT   regInputBuf[REG_INPUT_NREGS];

static bool bInitialized = false;

static void init_memory_space(void);

/**
 * @brief FreeRTOS task entry point for the Modbus RTU slave.
 *
 * Initialises the register memory space, configures and enables the freemodbus
 * stack using the compile-time defaults (RTU, slave address 0x0A, 115200 8N1),
 * then enters the infinite polling loop that drives the stack state machine.
 *
 * @param pvParameters  Unused FreeRTOS task parameter (pass NULL).
 */
void modbus_task(void *pvParameters)
{
    init_memory_space();

    // todo [2026-04-18] Check if flash has a setting saved
    (void)eMBInit(DEFAULT_MODE,       // mode
                  DEFAULT_SLAVE_ADDR, // slave address
                  0,                  // port - not used
                  DEFAULT_BAUDERATE,  // baude
                  DEFAULT_PARITY,     // parity
                  1u);                // stop bits

    (void)eMBEnable();

    while(1)
    {
        // Main polling loop for modbus stack
        (void)eMBPoll();

        // todo [2026-4-19] Really want to have a health task that checks that all tasks have checked in
        // then pet the dog BUT for now this is good enough
        watchdog_pet();
    }
}

/**
 * @brief Initialise the holding and input register buffers.
 *
 * Pre-fills each register with its own zero-based index value so that the
 * contents are deterministic before any master writes them.  Sets
 * @c bInitialized to signal that the buffers are ready for use by the
 * freemodbus callbacks.
 *
 * @note todo [2026-4-18] Get stored values in from flash.
 */
static void init_memory_space(void)
{
    // todo [2026-4-18] Get stored values in from flash

    // holding registers
    for(size_t idx = 0; idx < REG_HOLDING_NREGS; idx++)
    {
        regHoldingBuf[idx] = (uint16_t)idx;
    }

    // input registers
    for(size_t idx = 0; idx < REG_INPUT_NREGS; idx++)
    {
        regInputBuf[idx] = (uint16_t)idx;
    }

    bInitialized = true;
}

/**
 * @brief freemodbus callback — read input registers (function code 0x04).
 *
 * Called by the freemodbus stack when a master issues a Read Input Registers
 * request.  Copies the requested register values from @c regInputBuf into
 * @p pRegBuffer in big-endian byte order.
 *
 * @param pRegBuffer  Destination buffer managed by the freemodbus stack.
 * @param address     Starting Modbus register address.
 * @param nRegs       Number of registers requested.
 * @return MB_ENOERR  if the requested range lies within @c regInputBuf.
 * @return MB_ENOREG  if any part of the range is out of bounds.
 */
eMBErrorCode eMBRegInputCB( UCHAR * pRegBuffer, USHORT address, USHORT nRegs )
{
    assert(bInitialized);

    eMBErrorCode eStatus = MB_ENOERR;
    size_t iRegIndex;

    if( ( address >= REG_INPUT_START )
        && ( address + nRegs <= REG_INPUT_START + REG_INPUT_NREGS ) )
    {
        iRegIndex = ( int )( address - regInputStart );
        while( nRegs > 0 )
        {
            *pRegBuffer++ = ( unsigned char )( regInputBuf[iRegIndex] >> 8 );
            *pRegBuffer++ = ( unsigned char )( regInputBuf[iRegIndex] & 0xFF );
            iRegIndex++;
            nRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}

/**
 * @brief freemodbus callback — read/write holding registers (function codes 0x03 / 0x10).
 *
 * Called by the freemodbus stack for Read Holding Registers and Write Multiple
 * Registers requests.  On a read (@c MB_REG_READ) the register values are
 * copied from @c regHoldingBuf into @p pRegBuffer in big-endian order.  On a
 * write (@c MB_REG_WRITE) the big-endian bytes in @p pRegBuffer are assembled
 * back into 16-bit values and stored in @c regHoldingBuf.
 *
 * @param pRegBuffer  Source or destination buffer managed by the freemodbus stack.
 * @param address     Starting Modbus register address.
 * @param nRegs       Number of registers to read or write.
 * @param eMode       @c MB_REG_READ or @c MB_REG_WRITE.
 * @return MB_ENOERR  if the requested range lies within @c regHoldingBuf.
 * @return MB_ENOREG  if any part of the range is out of bounds.
 */
eMBErrorCode eMBRegHoldingCB( UCHAR * pRegBuffer, USHORT address, USHORT nRegs, eMBRegisterMode eMode )
{
    assert(bInitialized);

    eMBErrorCode eStatus = MB_ENOERR;
    size_t iRegIndex;

    if( (address >= REG_HOLDING_START) &&
        (address + nRegs <= REG_HOLDING_START + REG_HOLDING_NREGS ))
    {
        iRegIndex = (int)(address - regHoldingStart);
        switch (eMode)
        {
        // Pass current register values to the protocol stack.
        case MB_REG_READ:
            while(nRegs > 0)
            {
                *pRegBuffer++ = (uint8_t)(regHoldingBuf[iRegIndex] >> 8 );
                *pRegBuffer++ = (uint8_t)(regHoldingBuf[iRegIndex] & 0xFF);
                iRegIndex++;
                nRegs--;
            }
            break;

        // Update current register values with new values from the
        // protocol stack.
        case MB_REG_WRITE:
            while(nRegs > 0)
            {
                regHoldingBuf[iRegIndex]  = *pRegBuffer++ << 8;
                regHoldingBuf[iRegIndex] |= *pRegBuffer++;
                iRegIndex++;
                nRegs--;
            }
            break;
        default:
            // Not supported
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

/**
 * @brief freemodbus callback — read/write coil registers (function codes 0x01 / 0x0F).
 *
 * Coil registers are not implemented on this device.
 *
 * @param pRegBuffer  Unused.
 * @param address     Unused.
 * @param usNCoils    Unused.
 * @param eMode       Unused.
 * @return MB_ENOREG  always — coils are not supported.
 */
eMBErrorCode eMBRegCoilsCB( UCHAR * pRegBuffer, USHORT address, USHORT usNCoils, eMBRegisterMode eMode )
{
    return MB_ENOREG;
}

/**
 * @brief freemodbus callback — read discrete input registers (function code 0x02).
 *
 * Discrete inputs are not implemented on this device.
 *
 * @param pRegBuffer  Unused.
 * @param address     Unused.
 * @param usNDiscrete Unused.
 * @return MB_ENOREG  always — discrete inputs are not supported.
 */
eMBErrorCode eMBRegDiscreteCB( UCHAR * pRegBuffer, USHORT address, USHORT usNDiscrete )
{
    return MB_ENOREG;
}
