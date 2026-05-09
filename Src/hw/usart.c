/**
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  */

#include "usart.h"

#include "stm32l5xx_hal.h"
#include "usart_common.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


UART_HandleTypeDef huart2;
static UART_HandleTypeDef * const pHandle = &huart2;

UART_HandleTypeDef * USART2_GetHandle(void)
{
    return &huart2;
}

// @todo [2026-4-12] Can we utilize DMA here?  Freemodbus may need modification.
DMA_HandleTypeDef hdma_usart2_tx;

static void MX_USART2_UART_Init(uint32_t const baudeRate,
                                uint8_t  const dataBits,
                                uint8_t  const stopBits,
                                uint8_t  const parity);

/**
 * @brief Configure and initialize the USART2 HAL handle.
 *
 * Sets up the UART_HandleTypeDef with the given parameters and calls
 * HAL_UART_Init(). Also assigns the RX/TX ISR function pointers and
 * configures FIFO thresholds.
 *
 * @param baudeRate Baud rate in bits per second.
 * @param dataBits  Number of data bits: 7, 8, or 9.
 * @param stopBits  Number of stop bits: 1 or 2.
 * @param parity    Parity: 0 = none, 1 = odd, 2 = even.
 */
static void MX_USART2_UART_Init(uint32_t const baudeRate,
                                uint8_t  const dataBits,
                                uint8_t  const stopBits,
                                uint8_t  const parity)
{

  huart2.Instance = USART2;

  huart2.Init.BaudRate = baudeRate;

  if(7u == dataBits)
  {
    huart2.Init.WordLength = UART_WORDLENGTH_7B;
  }
  else if(8u == dataBits)
  {
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
  }
  else if(9u == dataBits)
  {
    huart2.Init.WordLength = UART_WORDLENGTH_9B;
  }
  else
  {
    bool const bBadWordLength = false;
    assert(bBadWordLength);
  }

  // Only support 1 and 2 stop bits right now
  if(1u == stopBits)
  {
    huart2.Init.StopBits = UART_STOPBITS_1;
  }
  else if(2u == stopBits)
  {
    huart2.Init.StopBits = UART_STOPBITS_2;
  }
  else
  {
    bool const bBadStopBits = false;
    assert(bBadStopBits);
  }

  if(0u == parity)
  {
    huart2.Init.Parity = UART_PARITY_NONE;
  }
  else if(1u == parity)
  {
    huart2.Init.Parity = UART_PARITY_ODD;
  }
  else if(2u == parity)
  {
    huart2.Init.Parity = UART_PARITY_EVEN;
  }
  else
  {
    bool const bBadParity = false;
    assert(bBadParity);
  }

  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;

  // @todo [2026-4-12] Create a feature to enable auto baudrate detection
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  assert(HAL_OK == HAL_UART_Init(&huart2));
  assert(HAL_OK == HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8));
  assert(HAL_OK == HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8));
  assert(HAL_OK == HAL_UARTEx_DisableFifoMode(&huart2));

  // todo [2026-4-19] Bug usart
  // Can't use a b_initialized as the MspInit is called via HAL_UART_Init.
  // Need better way.  Also I think modbus enable/disable functionality is going
  // to be different.
}

/**
 * @brief HAL UART MSP initialization callback.
 *
 * Called by HAL_UART_Init(). Configures the USART2 peripheral clock source,
 * enables USART2 and GPIOD clocks, initializes PD5 (TX) and PD6 (RX) as
 * alternate-function pins, and enables the USART2 interrupt in the NVIC.
 *
 * @param pUartHandle Pointer to the UART handle being initialized.
 */
void HAL_UART_MspInit(UART_HandleTypeDef * pUartHandle)
{
  assert(NULL != pUartHandle);

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if(USART2 == pUartHandle->Instance)
  {
    /** Initializes the peripherals clock*/
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
    assert(HAL_OK == HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit));

    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PD5     ------> USART2_TX
    PD6     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_MB_USAT_RX_Pin|GPIO_MB_USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USART2 DMA Init */
  /* USART2_TX Init */
  /* @todo [2026-4-12] Can we utilize DMA here?  Freemodbus may need modification.
  hdma_usart2_tx.Instance = DMA1_Channel3;
  hdma_usart2_tx.Init.Request = DMA_REQUEST_USART2_TX;
  hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_usart2_tx.Init.Mode = DMA_NORMAL;
  hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;

  assert(HAL_OK == HAL_DMA_Init(&hdma_usart2_tx));
  assert(HAL_OK == HAL_DMA_ConfigChannelAttributes(&hdma_usart2_tx, DMA_CHANNEL_NPRIV));

  __HAL_LINKDMA(pUartHandle,hdmatx,hdma_usart2_tx);
    */

    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  }
}

/**
 * @brief HAL UART MSP de-initialization callback.
 *
 * Called by HAL_UART_DeInit(). Disables the USART2 NVIC interrupt, disables
 * the peripheral clock, and deinitializes the GPIOD TX/RX pins.
 *
 * @param pUartHandle Pointer to the UART handle being de-initialized.
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef * pUartHandle)
{
  assert(NULL != pUartHandle);


  if(USART2 == pUartHandle->Instance)
  {
    /* USART2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART2_IRQn);

    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PD5     ------> USART2_TX
    PD6     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOD, GPIO_MB_USAT_RX_Pin|GPIO_MB_USART_RX_Pin);
  }
}

/**
 * @brief Initialize a USART peripheral with the given UART parameters.
 *
 * Dispatches to the appropriate HAL init routine based on usartNum.
 * Currently only USART2 is supported.
 *
 * @param usartNum  USART instance number (only 2 is supported).
 * @param baudRate  Baud rate in bits per second (e.g. 9600, 115200).
 * @param dataBits  Number of data bits: 7, 8, or 9.
 * @param stop_bits Number of stop bits: 1 or 2.
 * @param parity    Parity: 0 = none, 1 = odd, 2 = even.
 */
void usart_uart_init(uint8_t  const usartNum,
                     uint32_t const baudRate,
                     uint8_t  const dataBits,
                     uint8_t  const stop_bits,
                     uint8_t  const parity)
{

  (void)hdma_usart2_tx;
  
  if(2u == usartNum)
  {
    MX_USART2_UART_Init(baudRate, dataBits, stop_bits, parity);
  }
  else
  {
    bool bInvalidUsart = false;
    assert(bInvalidUsart);
  }

}

/**
 * @brief Enable a USART peripheral (GPIO, clocks, NVIC).
 *
 * Calls HAL_UART_MspInit() to configure GPIO pins, enable peripheral clocks,
 * and register the USART interrupt. Must be called after usart_uart_init().
 * Currently only USART2 is supported.
 *
 * @param usartNum USART instance number (only 2 is supported).
 */
void usart_uart_bringup(uint8_t const usartNum)
{
  // @todo [2026-4-12] Protect USART with a mutex
  if(2u == usartNum)
  {
    HAL_UART_MspInit(&huart2);
  }
  else
  {
    bool bInvalidUsart = false;
    assert(bInvalidUsart);
  }
}
/**
 * @brief Disable a USART peripheral and release its resources.
 *
 * Calls HAL_UART_MspDeInit() to disable the NVIC interrupt, peripheral
 * clock, and GPIO pins. Currently only USART2 is supported.
 *
 * @param usartNum USART instance number (only 2 is supported).
 */
void usart_uart_teardown(uint8_t const usartNum)
{
  // @todo [2026-4-12] Protect USART with a mutex
  if(2u == usartNum)
  {
    HAL_UART_MspDeInit(&huart2);
  }
  else
  {
      bool bInvalidUsart = false;
      assert(bInvalidUsart);
  }
}

/**
 * @brief Gate the USART RX interrupt (RXNEIE bit in CR1).
 *
 * Sets or clears USART_CR1_RXNEIE directly in the peripheral register.
 * The NVIC line for the USART remains active — only the per-interrupt
 * enable bit is touched, which is the correct granularity for Modbus
 * RX/TX direction switching on a half-duplex RS-485 bus.
 *
 * @param usartNum  USART instance number (only 2 is supported).
 * @param bEnable   true  — arm the RX interrupt (RXNEIE = 1).
 *                  false — mask the RX interrupt (RXNEIE = 0).
 */
void usart_uart_enable_rx(uint8_t const usartNum, bool const bEnable)
{
  // @todo [2026-4-12] Protect USART with a mutex
  if(2u == usartNum)
  {
    if(bEnable)
    {
      USART2->CR1 |= USART_CR1_RXNEIE;
    }
    else
    {
      USART2->CR1 &= ~USART_CR1_RXNEIE;
    }
  }
  else
  {
    bool bInvalidUsart = false;
    assert(bInvalidUsart);
  }
}

/**
 * @brief Disable TX interrupt.
 */
void usart_uart_enable_tx(uint8_t const usartNum, bool const bEnable)
{
  // @todo [2026-4-12] Protect USART with a mutex
  if(2u == usartNum)
  {
    if(bEnable)
    {
      USART2->CR1 |= USART_CR1_TXEIE;
    }
    else
    {
      USART2->CR1 &= ~USART_CR1_TXEIE;
    }
  }
  else
  {
    bool bInvalidUsart = false;
    assert(bInvalidUsart);
  }
}