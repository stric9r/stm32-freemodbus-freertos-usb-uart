#ifndef SYSTEM_TASK_H
#define SYSTEM_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSTEM_TASK_ID_MODBUS = 0,
    SYSTEM_TASK_ID_USB    = 1,
} SystemTaskId_t;

void system_task(void *arg);

void system_task_register(SystemTaskId_t id);
void system_task_check_in(SystemTaskId_t id);

#ifdef __cplusplus
}
#endif
#endif /* SYSTEM_TASK_H */
