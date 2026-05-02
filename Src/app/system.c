/**
  ******************************************************************************
  * File Name          : system.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "system.h"
#include "system_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "portserial.h"
#include "modbus_port_ownership.h"

#if COMMS_MODBUS_PORT != COMMS_MODBUS_USB
#include "modbus_task.h"
#endif

#if COMMS_MODBUS_PORT != COMMS_MODBUS_UART
#include "modbus_usb.h"
#include "usb_device.h"
#endif

#include "usb_task.h"

/* Idle task — required by configSUPPORT_STATIC_ALLOCATION */
static StaticTask_t idleTaskTcb;
static StackType_t  idleTaskStack[configMINIMAL_STACK_SIZE];

#if COMMS_MODBUS_PORT != COMMS_MODBUS_USB
/* Modbus UART task */
static TaskHandle_t modbusUartTaskHandle;
static StaticTask_t modbusUartTaskTcb;
static StackType_t  modbusUartTaskStack[MODBUS_TASK_STACK_SIZE];
#endif

#if COMMS_MODBUS_PORT != COMMS_MODBUS_UART
/* USB task */
static TaskHandle_t usbTaskHandle;
static StaticTask_t usbTaskTcb;
static StackType_t  usbTaskStack[USB_TASK_STACK_SIZE];
#endif

/* System task */
static StaticTask_t       systemTaskTcb;
static StackType_t        systemTaskStack[SYSTEM_TASK_STACK_SIZE];

void system_app_init(void)
{
    /* Always called — no-op in non-DYNAMIC builds (guard is inside the function) */
    modbus_port_ownership_init();


#if COMMS_MODBUS_PORT != COMMS_MODBUS_USB
    modbusUartTaskHandle = xTaskCreateStatic(
        modbus_task,
        "modbus_task",
        MODBUS_TASK_STACK_SIZE,
        NULL,
        MODBUS_TASK_PRIORITY,
        modbusUartTaskStack,
        &modbusUartTaskTcb);
#endif

#if COMMS_MODBUS_PORT != COMMS_MODBUS_UART
    usbTaskHandle = xTaskCreateStatic(
        usb_task,
        "usb_task",
        DEFAULT_TASK_STACK_SIZE,
        NULL,
        USB_TASK_PRIORITY,
        usbTaskStack,
        &usbTaskTcb);
#endif

    xTaskCreateStatic(
        system_task,
        "system_task",
        SYSTEM_TASK_STACK_SIZE,
        NULL,
        SYSTEM_TASK_PRIORITY,
        systemTaskStack,
        &systemTaskTcb);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &idleTaskTcb;
    *ppxIdleTaskStackBuffer = idleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
