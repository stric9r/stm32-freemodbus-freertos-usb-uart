/**
  ******************************************************************************
  * @file           : system_task.h
  * @brief          : Watchdog coordinator task: SystemTaskId_t enum
  *                   and check-in API
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef SYSTEM_TASK_H
#define SYSTEM_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSTEM_TASK_ID_MODBUS = 0,
    // Not used right now, USB with modbus is indirectly petting the watchdog
    SYSTEM_TASK_ID_USB    = 1,
} SystemTaskId_t;

void system_task(void *arg);

void system_task_register(SystemTaskId_t id);
void system_task_check_in(SystemTaskId_t id);

#ifdef __cplusplus
}
#endif
#endif /* SYSTEM_TASK_H */
