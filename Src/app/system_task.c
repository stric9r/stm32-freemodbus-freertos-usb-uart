/**
 * @file system_task.c
 * @brief Centralised watchdog health task.
 *
 * Each monitored task calls system_task_check_in() periodically.  Once every
 * registered task has checked in, the watchdog is petted and the event bits
 * are cleared for the next round.  If any task misses the WD_CHECK_IN_TIME_MS
 * window the hardware IWDG fires naturally.
 */

#include "system_task.h"
#include "system.h"
#include "watchdog.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

static EventGroupHandle_t checkinGroup;
static StaticEventGroup_t checkinGroupBuffer;
static EventBits_t        requiredBits;


/**
 * @brief System task — waits for all registered check-ins then pets the watchdog.
 */
void system_task(void *arg)
{
    (void)arg;

    checkinGroup = xEventGroupCreateStatic(&checkinGroupBuffer);

    for (;;)
    {
        EventBits_t const bits = xEventGroupWaitBits(
            checkinGroup,
            requiredBits,
            pdTRUE,   /* clear on exit */
            pdTRUE,   /* wait for ALL bits */
            pdMS_TO_TICKS(WD_CHECK_IN_TIME_MS));

        if (requiredBits == (bits & requiredBits))
        {
            watchdog_pet();
        }
    }
}

/**
 * @brief Register a task as required to check in each watchdog cycle.
 *        Must be called after system_task_init() and before the scheduler starts.
 *
 * @param id  The task's SystemTaskId_t.
 */
void system_task_register(SystemTaskId_t id)
{
    requiredBits |= (EventBits_t)(1u << id);
}

/**
 * @brief Signal that a task is healthy.  Safe to call from any task context.
 *
 * @param id  The calling task's SystemTaskId_t.
 */
void system_task_check_in(SystemTaskId_t id)
{
    xEventGroupSetBits(checkinGroup, (EventBits_t)(1u << id));
}
