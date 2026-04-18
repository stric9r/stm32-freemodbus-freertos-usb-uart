#include "mb.h"
#include "port.h"
#include "port_internal.h"
#include "usart.h"
#include "usart_common.h"

#include <stdlib.h>


BOOL xMBPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity, UCHAR ucStopBits)
{
    (void)ucPORT; // not using, HW port is USART 2

    uint8_t parity = 0;
    parity = (MB_PAR_ODD == eParity)  ? 1u : parity;
    parity = (MB_PAR_EVEN == eParity) ? 2u : parity;

    usart_uart_init(2u, (uint32_t)ulBaudRate, (uint8_t)ucDataBits, parity, (uint8_t)ucStopBits);

    return TRUE;
}


void vMBPortSerialEnable(BOOL rxEnable, BOOL txEnable)
{
	// @todo [2026-4-12] Update USART function to allow RX/TX options for enable/disable
	usart_uart_bringup(2u);
}

BOOL xMBPortSerialPutByte(CHAR byte)
{
	// @todo [2026-4-12] Can we use DMA to do the TX's?
    usart_uart2_set_byte(byte);
    return TRUE;
}

BOOL xMBPortSerialGetByte(CHAR *byte)
{
    *byte = usart_uart2_get_byte();
    return TRUE;
}

void USART2_IRQRXHandler(UART_HandleTypeDef *huart)
{
	vMBTimerDebugSetLow();
	pxMBFrameCBByteReceived();

}

void USART2_IRQTXHandler(UART_HandleTypeDef *huart)
{
	pxMBFrameCBTransmitterEmpty();
}

/* ----------------------- End of file --------------------------------------*/

