#include "mb.h"
#include "port.h"
#include "port_internal.h"

/**
 * @brief  Initialize the FreeModbus serial port.
 *
 * Converts the FreeModbus parity enum to the numeric value expected by the
 * usart layer (0 = none, 1 = odd, 2 = even), then initializes MB_SERIAL
 * with the requested line parameters. The @p ucPORT argument is unused
 * because the hardware USART is fixed by MB_SERIAL in port_internal.h.
 *
 * @param  ucPORT      Unused — hardware port is fixed by MB_SERIAL.
 * @param  ulBaudRate  Baud rate in bits per second.
 * @param  ucDataBits  Number of data bits: 7, 8, or 9.
 * @param  eParity     FreeModbus parity enum (MB_PAR_NONE/ODD/EVEN).
 * @param  ucStopBits  Number of stop bits: 1 or 2.
 * @return TRUE always.
 */
BOOL xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits,
                        eMBParity eParity, UCHAR ucStopBits )
{
    (void)ucPORT;

    uint8_t parity = 0u;
    parity = (MB_PAR_ODD  == eParity) ? 1u : parity;
    parity = (MB_PAR_EVEN == eParity) ? 2u : parity;

    MB_SERIAL_INIT((uint32_t)ulBaudRate, (uint8_t)ucDataBits,
                   (uint8_t)ucStopBits, parity);

    MB_SERIAL_ENABLE_RX_IRQ();

    return TRUE;
}

/**
 * @brief  Gate the MB_SERIAL RX and TX interrupts independently.
 *
 * Called by FreeModbus to switch between receive and transmit modes.
 * For half-duplex RS-485, the stack drives the direction pin by enabling
 * only one interrupt at a time:
 *   - Start of reception: rxEnable=TRUE,  txEnable=FALSE
 *   - Start of transmission: rxEnable=FALSE, txEnable=TRUE
 *
 * Interrupt enable/disable is done by writing RXNEIE and TXEIE directly
 * in CR1 rather than through the HAL, matching the lean interrupt approach
 * used in MB_SERIAL_IRQ_FUNC(). The peripheral clocks, GPIO, and NVIC are
 * brought up once during xMBPortSerialInit() and remain active.
 *
 * @param  rxEnable  TRUE to arm the RX interrupt; FALSE to mask it.
 * @param  txEnable  TRUE to arm the TX interrupt; FALSE to mask it.
 */
void vMBPortSerialEnable( BOOL rxEnable, BOOL txEnable )
{
    if (rxEnable)
    {
        MB_SERIAL_ENABLE_RX_IRQ();
    }
    else
    {
        MB_SERIAL_DISABLE_RX_IRQ();
    }

    if (txEnable)
    {
        MB_SERIAL_ENABLE_TX_IRQ();
    }
    else
    {
        MB_SERIAL_DISABLE_TX_IRQ();
    }
}

/**
 * @brief  Write one byte to the MB_SERIAL transmit data register.
 *
 * Writes directly to the peripheral TDR, bypassing the HAL to avoid
 * per-byte overhead on the hot transmit path.
 *
 * @param  byte  Byte to transmit.
 * @return TRUE always.
 */
BOOL xMBPortSerialPutByte( CHAR byte )
{
    MB_SERIAL_PUT_BYTE(byte);
    return TRUE;
}

/**
 * @brief  Read one byte from the MB_SERIAL receive data register.
 *
 * Reads directly from the peripheral RDR, bypassing the HAL to avoid
 * per-byte overhead on the hot receive path.
 *
 * @param  byte  Pointer to store the received byte.
 * @return TRUE always.
 */
BOOL xMBPortSerialGetByte( CHAR *byte )
{
    *byte = (CHAR)MB_SERIAL_GET_BYTE();
    return TRUE;
}

/**
 * @brief  MB_SERIAL hardware interrupt handler.
 *
 * Owns the vector-table entry for the USART peripheral used by Modbus
 * (currently USART2, via MB_SERIAL_PERIPH_IRQ). HAL_UART_IRQHandler() is
 * intentionally not called here. The HAL dispatcher runs ~15 flag checks,
 * dispatches through function pointers, and manages DMA bookkeeping — none
 * of which apply to the FreeModbus byte-at-a-time model. On the Modbus hot
 * path every received character triggers this ISR, so that overhead is
 * wasted cycles on every single byte.
 *
 * MB_SERIAL_IRQ_FUNC() reads ISR once, clears the error flags that would
 * stall the UART (framing, noise, overrun), and calls the FreeModbus
 * callbacks only for the interrupts that are actually enabled in CR1.
 * See the macro definition in port_internal.h for the full rationale.
 */
void MB_SERIAL_PERIPH_IRQ( void )
{
    MB_SERIAL_IRQ_FUNC();
}
