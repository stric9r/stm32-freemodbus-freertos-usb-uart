/**
  ******************************************************************************
  * @file           : usb_task.c
  * @brief          : USB CDC task: initialises USB device and runs the Modbus USB adapter
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#include "usb_task.h"
#include "usb_device.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "portserial.h"

#if COMMS_MODBUS_PORT != COMMS_MODBUS_UART
#include "modbus_usb.h"
#else
#include "main.h"
#include "system_task.h"
#endif

/**
 * @brief USB task — initialises the USB CDC port then runs the Modbus USB handler.
 *
 * modbus_usb_init() must be called before MX_USB_Device_Init() so the RX stream
 * buffer exists before the first USB ISR fires.
 *
 * USBD_malloc is mapped to USBD_static_malloc (Inc/hw/usbd_conf.h) — no
 * FreeRTOS heap dependency; safe to call from task context.
 */
void usb_task(void * argument)
{
    (void)argument;

    #if COMMS_MODBUS_PORT != COMMS_MODBUS_UART
    modbus_usb_init();
    #endif

    MX_USB_Device_Init();
    for (;;)
    {
        #if COMMS_MODBUS_PORT != COMMS_MODBUS_UART
        (void)modbus_usb_run();
        #else
        vTaskDelay(pdMS_TO_TICKS(SYSTEM_CHECK_IN_TIME_MS));
        system_task_check_in(SYSTEM_TASK_ID_USB);
        #endif
    }
}