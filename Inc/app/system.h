#ifndef SYSTEM_H
#define SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Task stack sizes — in words (1 word = 4 bytes on Cortex-M) */
#define BASE_STACK_SIZE           512u
#define DEFAULT_TASK_STACK_SIZE   (BASE_STACK_SIZE)
#define MODBUS_TASK_STACK_SIZE    (BASE_STACK_SIZE)  /* 100 is arbitrary right now - depends on your task */
#define USB_TASK_STACK_SIZE       (BASE_STACK_SIZE * 2u)  /* USB middleware needs ~1.5-2KB headroom */

/* Task priorities */
#define TASK_IDLE_PRIORITY        (0) /* Just for reference*/
#define DEFAULT_TASK_PRIORITY     (4) /* Arbitrarily chosen */
#define MODBUS_TASK_PRIORITY      (5)


void system_app_init(void);

#ifdef __cplusplus
}
#endif
#endif /* SYSTEM_H */
