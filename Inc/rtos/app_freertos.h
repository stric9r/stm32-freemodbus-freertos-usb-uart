#ifndef APP_FREERTOS_H
#define APP_FREERTOS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Task stack sizes — in words (1 word = 4 bytes on Cortex-M) */
#define DEFAULT_TASK_STACK_SIZE   128u
#define MODBUS_TASK_STACK_SIZE    (DEFAULT_TASK_STACK_SIZE + 100u)

/* Task priorities */
#define TASK_IDLE_PRIORITY        (0) /* Just for reference*/
#define DEFAULT_TASK_PRIORITY     (4) /* Arbitrarily chosen */
#define MODBUS_TASK_PRIORITY      (5)


void app_freertos_init(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_FREERTOS_H */
