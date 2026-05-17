/******************************************************************************
 * @file iwdg_feeder_task.c
 *
 * @par dependencies
 * - osal_wrapper_adapter.h
 * - watchdog_port.h  (MCU port — single mcu_watchdog_refresh() call)
 *
 * @author Ethan-Hang
 *
 * @brief Always-on task that refreshes the independent watchdog.
 *
 *        Bootloader's OTA_StateManager enables IWDG before jumping into the
 *        APP for the CHECK_START → CHECKING flow. STM32F411 IWDG is fused-on
 *        once started — it cannot be disabled by software, only a power-on
 *        reset clears it. So this task runs forever on EVERY boot, not only
 *        post-OTA, to avoid an accidental rollback on a normal boot that
 *        happens to follow an OTA cycle.
 *
 *        Bootloader-side reload is ~6 s (prescaler 64, reload 3000). We feed
 *        every 500 ms — 12× safety margin against scheduler hiccups.
 *
 *        IWDG register access lives in the MCU port (watchdog_port.c); this
 *        file owns only the cadence and the task wrapper.
 *
 * @version V1.1 2026-05-17
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_wrapper_adapter.h"
#include "os_freertos.h"

#include "watchdog_port.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define IWDG_FEEDER_PERIOD_MS  (500U)
//******************************** Defines **********************************//

//******************************* Functions *********************************//
void iwdg_feeder_task(void *argument)
{
    (void)argument;

    /**
    * Always-on feeder. PRI_BACKGROUND keeps it well below any business
    * task; if higher-priority work starves us for >6 s the IWDG will fire
    * and trigger the bootloader's rollback path — which is the correct
    * behaviour for "APP wedged".
    **/
    for (;;)
    {
        mcu_watchdog_refresh();
        osal_task_delay(OS_MS_TO_TICKS(IWDG_FEEDER_PERIOD_MS));
    }
}
//******************************* Functions *********************************//
