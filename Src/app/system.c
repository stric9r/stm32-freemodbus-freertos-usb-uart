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

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "modbus_task.h"

/* Default task */
static TaskHandle_t defaultTaskHandle;
static StaticTask_t defaultTaskTcb;
static StackType_t  defaultTaskStack[DEFAULT_TASK_STACK_SIZE];

/* Modbus task */
static TaskHandle_t modbusTaskHandle;
static StaticTask_t modbusTaskTcb;
static StackType_t  modbusTaskStack[MODBUS_TASK_STACK_SIZE];

/* Idle task — required by configSUPPORT_STATIC_ALLOCATION */
static StaticTask_t idleTaskTcb;
static StackType_t  idleTaskStack[configMINIMAL_STACK_SIZE];

static void defaultTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void system_app_init(void)
{
    defaultTaskHandle = xTaskCreateStatic(
        defaultTask,
        "defaultTask",
        DEFAULT_TASK_STACK_SIZE,
        NULL,
        DEFAULT_TASK_PRIORITY,
        defaultTaskStack,
        &defaultTaskTcb);

    modbusTaskHandle = xTaskCreateStatic(
        modbus_task,
        "modbusTask",
        MODBUS_TASK_STACK_SIZE,
        NULL,
        MODBUS_TASK_PRIORITY,
        modbusTaskStack,
        &modbusTaskTcb);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &idleTaskTcb;
    *ppxIdleTaskStackBuffer = idleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

