/******************************************************************************
 * @file iwdg_feeder_task.c
 *
 * @par dependencies
 * - osal_wrapper_adapter.h
 * - upgrade_service.h
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
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_wrapper_adapter.h"
#include "os_freertos.h"

#include "upgrade_service.h"
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
        ota_iwdg_refresh();
        osal_task_delay(OS_MS_TO_TICKS(IWDG_FEEDER_PERIOD_MS));
    }
}
//******************************* Functions *********************************//
