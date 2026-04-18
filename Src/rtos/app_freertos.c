/**
  ******************************************************************************
  * File Name          : freertos.c
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
#include "app_freertos.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#define DEFAULT_TASK_STACK_SIZE  128u

static TaskHandle_t defaultTaskHandle;
static StaticTask_t defaultTaskTcb;
static StackType_t  defaultTaskStack[DEFAULT_TASK_STACK_SIZE];

static StaticTask_t idleTaskTcb;
static StackType_t  idleTaskStack[configMINIMAL_STACK_SIZE];

static void defaultTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_freertos_init(void)
{
    defaultTaskHandle = xTaskCreateStatic(
        defaultTask,
        "defaultTask",
        DEFAULT_TASK_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1u,
        defaultTaskStack,
        &defaultTaskTcb);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &idleTaskTcb;
    *ppxIdleTaskStackBuffer = idleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

