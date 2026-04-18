#include "modbus_task.h"
#include "mb.h"

#include "assert.h"

#include "stm32l5xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stddef.h>

#define DEFAULT_MODE                MB_RTU
#define DEFAULT_SLAVE_ADDR          0x0A
#define DEFAULT_BAUDERATE           115200
#define DEFAULT_PARITY              MB_PAR_NONE


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

static void modbus_task(void *pvParameters);
static void init_memory_space(void);

void modbus_task(void *pvParameters)
{
    init_memory_space();

    // todo [2026-04-18] Check if flash has a setting saved
    (void)eMBInit(DEFAULT_MODE,       // mode
                  DEFAULT_SLAVE_ADDR, // slave address
                  0,                  // port - not used
                  DEFAULT_BAUDERATE,  // baude
                  DEFAULT_PARITY );   // parity

    (void)eMBEnable( );
    
    for( ;; )
    {
        // Main polling loop for modbus stack
        (void)eMBPoll();

        // todo [2026-4-18] Have a task notification on RX interrupt
    }
}

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

// This is from the third party examples
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

// This is from the third party examples
eMBErrorCode eMBRegCoilsCB( UCHAR * pRegBuffer, USHORT address, USHORT usNCoils, eMBRegisterMode eMode )
{
    return MB_ENOREG;
}

// This is from the third party examples
eMBErrorCode eMBRegDiscreteCB( UCHAR * pRegBuffer, USHORT address, USHORT usNDiscrete )
{
    return MB_ENOREG;
}


