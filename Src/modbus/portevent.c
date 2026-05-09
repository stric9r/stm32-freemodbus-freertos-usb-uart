/**
  ******************************************************************************
  * @file           : portevent.c
  * @brief          : FreeModbus port: event queue using a statically-allocated FreeRTOS queue
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#include "mb.h"
#include "mbport.h"

#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"

/**
 * @brief  FreeModbus event queue.
 *
 * Replaces the original static-flag implementation with a statically-allocated
 * FreeRTOS queue. This solves two problems with the flag approach:
 *
 *   1. Thread safety — xMBPortEventPost() is called from both ISR context
 *      (xMBRTUTimerT35Expired via the LPTIM ISR, xMBRTUTransmitFSM via the
 *      USART TX ISR) and task context (eMBPoll() posts EV_EXECUTE). The queue
 *      API handles both safely via xQueueSendFromISR / xQueueSend.
 *
 *   2. CPU efficiency — xMBPortEventGet() now blocks on xQueueReceive with
 *      portMAX_DELAY, so the Modbus task sleeps until an event is posted.
 *      No tight polling, no wasted cycles between frames.
 *
 * Queue depth of 4 covers all events that can be in-flight simultaneously:
 *   EV_READY, EV_FRAME_RECEIVED, EV_EXECUTE, EV_FRAME_SENT.
 */
#define EVENT_QUEUE_LENGTH      4u

static StaticQueue_t eventQueueStorage;
static uint8_t       eventQueueBuf[EVENT_QUEUE_LENGTH * sizeof(eMBEventType)];
static QueueHandle_t eventQueue;

BOOL xMBPortEventInit(void)
{
    eventQueue = xQueueCreateStatic(EVENT_QUEUE_LENGTH,
                                    sizeof(eMBEventType),
                                    eventQueueBuf,
                                    &eventQueueStorage);
    return TRUE;
}

/**
 * @brief  Post an event to the Modbus event queue.
 *
 * Safe to call from both ISR and task context. xPortIsInsideInterrupt()
 * reads the Cortex-M IPSR register: non-zero means an ISR is active and
 * the FromISR variant must be used. portYIELD_FROM_ISR triggers an
 * immediate context switch if a higher-priority task was unblocked.
 */
BOOL xMBPortEventPost(eMBEventType eEvent)
{
    BaseType_t xResult;

    if (xPortIsInsideInterrupt())
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xResult = xQueueSendFromISR(eventQueue, &eEvent, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xResult = xQueueSend(eventQueue, &eEvent, 0u);
    }

    return (pdTRUE == xResult) ? TRUE : FALSE;
}

/**
 * @brief  Retrieve the next event, blocking until one is available or the
 *         timeout expires.
 *
 * Called by eMBPoll() on every iteration. Blocks for up to
 * WD_CHECK_IN_TIME_MS so the Modbus task yields the CPU when there is
 * nothing to process, but wakes up periodically to check in with system
 * so it can pet the watchdog.
 *
 * @return TRUE if an event was received, FALSE if the receive timed out.
 */
BOOL xMBPortEventGet(eMBEventType *eEvent)
{
    return (pdTRUE == xQueueReceive(eventQueue, eEvent,
                                    pdMS_TO_TICKS(SYSTEM_CHECK_IN_TIME_MS)))
           ? TRUE : FALSE;
}
