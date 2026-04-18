#ifndef _PORT_INTERNAL_H
#define _PORT_INTERNAL_H

#include "stm32l5xx_hal.h"
#include "gpio.h"
#include "lptim.h"
#include "main.h"
#include "usart.h"
#include "usart_common.h"

/**
 * @defgroup MBTimer  Modbus hardware timer configuration
 *
 * These defines bind the Modbus port layer to a specific LPTIM instance
 * without scattering hardware names through porttimer.c. To migrate to a
 * different timer, update only this block:
 *
 *   MB_TIMER            — timer number passed to the lptim_*() API (1 = LPTIM1)
 *   MB_TIMER_IRQ        — name of the IRQ handler function in the vector table
 *   MB_TIMER_INSTANCE   — peripheral base-address pointer used for direct
 *                         register access in the IRQ body (bypasses HAL to
 *                         avoid dispatcher overhead — see MB_TIMER_IRQ_FUNC)
 *   MB_TIMER_ISR_FLAG   — ISR flag that indicates a period expiry (ARRM)
 *   MB_TIMER_ICR_FLAG   — ICR bit that clears the expiry flag
 *
 *   MB_TIMER_INIT()           — initialize the timer hardware
 *   MB_TIMER_SET_PERIOD(us)   — configure the one-shot timeout in microseconds
 *   MB_TIMER_ENABLE()         — arm the timer (gates clock on, starts countdown)
 *   MB_TIMER_DISABLE()        — disarm the timer (stops countdown, gates clock off)
 *
 * Current mapping: LPTIM1
 * @{
 */
#define MB_TIMER            1u
#define MB_TIMER_IRQ        LPTIM1_IRQHandler
#define MB_TIMER_INSTANCE   LPTIM1
#define MB_TIMER_ISR_FLAG   LPTIM_ISR_ARRM
#define MB_TIMER_ICR_FLAG   LPTIM_ICR_ARRMCF

#define MB_TIMER_INIT()                  lptim_init()
#define MB_TIMER_SET_PERIOD(timerUs)     lptim_set_period(MB_TIMER, (timerUs))
#define MB_TIMER_ENABLE()                lptim_enable(MB_TIMER)
#define MB_TIMER_DISABLE()               lptim_disable(MB_TIMER)
/** @} */

/**
 * @brief  Modbus timer expiry interrupt body.
 *
 * Checks the auto-reload match flag on MB_TIMER_INSTANCE, clears it
 * immediately to prevent spurious re-entry, then notifies FreeModbus that
 * the t3.5 inter-character silence period has elapsed.
 *
 * Implemented as a macro so MB_TIMER_IRQ (the actual vector-table function)
 * can expand it inline with zero call overhead.
 */
#define MB_TIMER_IRQ_FUNC()                              \
    {                                                    \
        if (MB_TIMER_INSTANCE->ISR & MB_TIMER_ISR_FLAG)  \
        {                                                \
            MB_TIMER_INSTANCE->ICR = MB_TIMER_ICR_FLAG;  \
            vMBTimerDebugSetHigh();                      \
            pxMBPortCBTimerExpired();                    \
            vMBTimerDebugSetLow();                       \
        }                                                \
    }
/**
 * @defgroup MBSerial  Modbus hardware serial configuration
 *
 * These defines bind the Modbus port layer to a specific USART instance
 * without scattering hardware names through portserial.c. To migrate to a
 * different USART, update only this block:
 *
 *   MB_SERIAL               — USART number passed to the usart_*() API (2 = USART2)
 *   MB_SERIAL_INSTANCE      — peripheral base-address pointer for direct TDR/RDR
 *                             register access (bypasses HAL for single-byte I/O)
 *   MB_SERIAL_PERIPH_IRQ    — hardware interrupt vector name in the vector table
 *
 *   MB_SERIAL_INIT(baud, dataBits, stopBits, parity) — initialize the peripheral
 *   MB_SERIAL_ENABLE()      — bring up clocks, GPIO, and NVIC
 *   MB_SERIAL_DISABLE()     — tear down clocks, GPIO, and NVIC
 *   MB_SERIAL_PUT_BYTE(b)   — write one byte directly to the TX data register
 *   MB_SERIAL_GET_BYTE()    — read one byte directly from the RX data register
 *
 *   MB_SERIAL_ENABLE_RX_IRQ()   — set USART_CR1_RXNEIE to arm the RX interrupt
 *   MB_SERIAL_DISABLE_RX_IRQ()  — clear USART_CR1_RXNEIE to mask the RX interrupt
 *   MB_SERIAL_ENABLE_TX_IRQ()   — set USART_CR1_TXEIE to arm the TX interrupt
 *   MB_SERIAL_DISABLE_TX_IRQ()  — clear USART_CR1_TXEIE to mask the TX interrupt
 *
 * Current mapping: USART2
 * @{
 */
#define MB_SERIAL                  2u
#define MB_SERIAL_INSTANCE         USART2
#define MB_SERIAL_PERIPH_IRQ       USART2_IRQHandler

#define MB_SERIAL_INIT(baud, dataBits, stopBits, parity) \
    usart_uart_init(MB_SERIAL, (baud), (dataBits), (stopBits), (parity))

#define MB_SERIAL_ENABLE()         usart_uart_bringup(MB_SERIAL)
#define MB_SERIAL_DISABLE()        usart_uart_teardown(MB_SERIAL)
#define MB_SERIAL_PUT_BYTE(b)      (MB_SERIAL_INSTANCE->TDR = (uint8_t)(b))
#define MB_SERIAL_GET_BYTE()       ((uint8_t)(MB_SERIAL_INSTANCE->RDR))

#define MB_SERIAL_ENABLE_RX_IRQ()  (MB_SERIAL_INSTANCE->CR1 |=  USART_CR1_RXNEIE)
#define MB_SERIAL_DISABLE_RX_IRQ() (MB_SERIAL_INSTANCE->CR1 &= ~USART_CR1_RXNEIE)
#define MB_SERIAL_ENABLE_TX_IRQ()  (MB_SERIAL_INSTANCE->CR1 |=  USART_CR1_TXEIE)
#define MB_SERIAL_DISABLE_TX_IRQ() (MB_SERIAL_INSTANCE->CR1 &= ~USART_CR1_TXEIE)
/** @} */

/**
 * @brief  Modbus serial interrupt body.
 *
 * Bypasses HAL_UART_IRQHandler() entirely. The HAL dispatcher checks ~15
 * status flags, dispatches through function pointers, and handles DMA
 * transfers — none of which are relevant to the FreeModbus byte-at-a-time
 * model. On a Modbus hot path (every received character re-arms the t3.5
 * timer) that overhead is wasted cycles on every single byte.
 *
 * This macro reads ISR once, clears the three error flags that would stall
 * the UART if left set (framing, noise, overrun), then calls the FreeModbus
 * callbacks only for the interrupts that are actually enabled in CR1. The
 * CR1 guard is required because vMBPortSerialEnable() uses USART_CR1_RXNEIE/TXEIE
 * to gate RX and TX independently for half-duplex RS-485 direction control.
 *
 * Implemented as a macro so MB_SERIAL_PERIPH_IRQ (the actual vector-table
 * function) can expand it inline with zero call overhead.
 */
#define MB_SERIAL_IRQ_FUNC()                                                      \
    {                                                                             \
        uint32_t const isr = MB_SERIAL_INSTANCE->ISR;                             \
        MB_SERIAL_INSTANCE->ICR = USART_ICR_FECF | USART_ICR_NECF                 \
                                  | USART_ICR_ORECF;                              \
        if ((isr & USART_ISR_RXNE) &&                                       \
            (MB_SERIAL_INSTANCE->CR1 & USART_CR1_RXNEIE))                 \
        {                                                                         \
            vMBTimerDebugSetLow();                                                \
            pxMBFrameCBByteReceived();                                            \
        }                                                                         \
        if ((isr & USART_ISR_TXE) &&                                        \
            (MB_SERIAL_INSTANCE->CR1 & USART_CR1_TXEIE))                  \
        {                                                                         \
            pxMBFrameCBTransmitterEmpty();                                        \
        }                                                                         \
    }

/* Debug helper functions */
#if MB_TIMER_DEBUG == 1
static inline void vMBTimerDebugSetHigh( void )
{
    HAL_GPIO_WritePin(GPIO_RED_LED_GPIO_Port, GPIO_RED_LED_Pin, GPIO_PIN_SET);
}

static inline void vMBTimerDebugSetLow( void )
{
    HAL_GPIO_WritePin(GPIO_RED_LED_GPIO_Port, GPIO_RED_LED_Pin, GPIO_PIN_RESET);
}
#else
#define vMBTimerDebugSetHigh()
#define vMBTimerDebugSetLow()
#endif


#endif // _PORT_INTERNAL_H
