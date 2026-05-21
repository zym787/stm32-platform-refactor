/******************************************************************************
 * @file bsp_wt588_handler.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Implement WT588 handler initialization and worker thread routines.
 *
 * Processing flow:
 * Instantiate driver, create queues/tasks, and clean up on init failure.
 * @version V1.0 2026-4-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wt588_handler.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define WT588_BUSY_POLL_CNT      (500U)
#define WT588_BUSY_POLL_MS       (3U)
#define WT588_MAX_PLAY_RETRY     (3U)

#define WT588_EXECUTOR_STACK_DEPTH   (512U)
#define WT588_EXECUTOR_PRIORITY      (24U)
#define WT588_BUSY_DET_STACK_DEPTH   (512U)
#define WT588_BUSY_DET_PRIORITY      (24U)

#define HANDLER_OS_INTERFACE(INST) ((INST)->handler_input_args->p_os_interface)
#define    HANLDER_LINK_LIST(INST) ((INST).p_play_request_list)

static bsp_wt588_handler_t s_wt588_handler_inst         = {0};
static list_handler_t      s_play_request_list_instance = {0};
static volatile bool       s_stop_requested             = false;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Main WT588 handler thread
 *
 * Manages play request queues, priority-based scheduling, and coordination
 * between executor and busy detection threads.
 *
 * @param[in] args : Pointer to handler input arguments
 *
 * @return void
 *
 * */
void wt588_handler_thread(void * const args)
{
    wt_handler_input_args_t *p_input_args = (wt_handler_input_args_t *)args;

    static bsp_wt588_driver_t wt588_driver_instance = {0};
    s_wt588_handler_inst.p_wt588_driver                   = \
                                          &wt588_driver_instance;
    s_wt588_handler_inst.semaphore_handler                = NULL;
    s_wt588_handler_inst.stop_semaphore                   = NULL;
    s_wt588_handler_inst.stop_ack_semaphore               = NULL;
    s_wt588_handler_inst.voice_add_queue                  = NULL;
    s_wt588_handler_inst.executor_queue                   = NULL;
    s_wt588_handler_inst.busy_detection_queue             = NULL;
    s_wt588_handler_inst.voice_finish_queue               = NULL;
    s_wt588_handler_inst.executor_thread_handle           = NULL;
    s_wt588_handler_inst.busy_detection_thread_handle     = NULL;
    s_wt588_handler_inst.p_play_request_list              = \
                                  &s_play_request_list_instance;

    wt_handler_status_t ret = wt588_handler_inst(&s_wt588_handler_inst,
                                                          p_input_args);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                    "wt588_handler_thread inst failed, task suspended");
        while (1)
        {
            osal_task_delay(1000);
        }
    }

    uint32_t            add_cnt         =     0;
    uint32_t            finish_cnt      =     0;
    list_voice_node_t  *p_node_add      =  NULL;
    list_voice_node_t  *p_node_del      =  NULL;
    list_voice_node_t  *p_node_highest  =  NULL;
    bool                is_higher_node  = false;
    static uint8_t      s_prev_cur_pri  =   15U;
    static uint8_t      s_prev_high_pri =   15U;

    DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "wt588_handler_thread running");
    while (1)
    {
        // Check for stop request
        wt_handler_status_t stop_ret = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
            ->pf_os_sema_take(s_wt588_handler_inst.stop_semaphore, 0);
        if (WT_HANDLER_OK == stop_ret)
        {
            DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "stop request received");

            // Drain voice_add_queue and free unprocessed nodes (not in list yet)
            list_voice_node_t      *p_drain = NULL;
            wt_os_heap_interface_t *p_heap  = HANDLER_OS_INTERFACE(
                                    &s_wt588_handler_inst)->p_os_heap_interface;
            uint32_t drain_cnt = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->
                pf_os_queue_count(s_wt588_handler_inst.voice_add_queue);
            for (uint32_t j = 0; j < drain_cnt; j++)
            {
                HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_get(
                    s_wt588_handler_inst.voice_add_queue, &p_drain, 0);
                p_heap->pf_os_free(p_drain);
            }

            // Drain voice_finish_queue (executor may have finished before stop)
            drain_cnt = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->
                pf_os_queue_count(s_wt588_handler_inst.voice_finish_queue);
            for (uint32_t j = 0; j < drain_cnt; j++)
            {
                HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_get(
                    s_wt588_handler_inst.voice_finish_queue, &p_drain, 0);
            }

            // Notify busy_detection to reset (reuse existing semaphore_handler)
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->
                pf_os_sema_give(s_wt588_handler_inst.semaphore_handler);

            // Send NULL sentinel: unblocks executor if it is idle on queue_get
            list_voice_node_t *p_sentinel = NULL;
            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_send(
                s_wt588_handler_inst.executor_queue, &p_sentinel, OSAL_MAX_DELAY);

            // Wait for executor to confirm it has stopped (2 s timeout)
            // Safe to free list nodes only after executor releases its pointer
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_sema_take(
                s_wt588_handler_inst.stop_ack_semaphore, 2000U);

            // Now safe to clear all list nodes
            list_voice_node_t *p_clear = NULL;
            while (!HANLDER_LINK_LIST(s_wt588_handler_inst)->pf_list_is_empty(
                        HANLDER_LINK_LIST(s_wt588_handler_inst)))
            {
                p_clear = HANLDER_LINK_LIST(s_wt588_handler_inst)->pf_get_first_node(
                              HANLDER_LINK_LIST(s_wt588_handler_inst));
                HANLDER_LINK_LIST(s_wt588_handler_inst)->pf_list_del_node(
                    HANLDER_LINK_LIST(s_wt588_handler_inst), p_clear);
            }

            // Reset priority state
            HANLDER_LINK_LIST(s_wt588_handler_inst)->current_priority  = 15U;
            HANLDER_LINK_LIST(s_wt588_handler_inst)->highest_priority  = 15U;

            DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "stop complete, resources cleared");
            continue;
        }

        // Check for new play requests and add them to the list
        add_cnt = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->\
                    pf_os_queue_count(s_wt588_handler_inst.voice_add_queue);
        if (add_cnt > 0)
        {
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "new play request count in queue: %lu", add_cnt);
        }

        // add new play requests to the list and sort by priority
        for (uint32_t i = 0; i < add_cnt; i++)
        {
            // get node from queue
            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->\
            pf_os_queue_get(s_wt588_handler_inst.voice_add_queue,
                                                     &p_node_add,
                                                  OSAL_MAX_DELAY);

            // add to list and sort if has more than 1 node in this priority level
            HANLDER_LINK_LIST(s_wt588_handler_inst)->pf_list_add_node(
                     HANLDER_LINK_LIST(s_wt588_handler_inst), p_node_add);
            if (HANLDER_LINK_LIST(s_wt588_handler_inst)->\
                              node_count_by_priority[p_node_add->priority] > 1)
            {
                HANLDER_LINK_LIST(s_wt588_handler_inst)->pf_list_sort(
                    HANLDER_LINK_LIST(s_wt588_handler_inst),
                    HANLDER_LINK_LIST(s_wt588_handler_inst)->current_priority);
            }
        }

        // delete finished play requests from the list
        finish_cnt =
            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
                ->pf_os_queue_count(s_wt588_handler_inst.voice_finish_queue);
        if (finish_cnt > 0)
        {
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "delete play request count in queue: %lu", finish_cnt);
        }
        for (uint32_t i = 0; i < finish_cnt; i++)
        {
            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
                ->pf_os_queue_get(s_wt588_handler_inst.voice_finish_queue,
                                  &p_node_del, OSAL_MAX_DELAY);
            HANLDER_LINK_LIST(s_wt588_handler_inst)
                ->pf_list_del_node(HANLDER_LINK_LIST(s_wt588_handler_inst),
                                   p_node_del);
        }

        // check if the highest priority play request has changed,
        // if so, send the new highest priority request to executor
        is_higher_node =
            HANLDER_LINK_LIST(s_wt588_handler_inst)->current_priority >
            HANLDER_LINK_LIST(s_wt588_handler_inst)->highest_priority;
        if (is_higher_node || 0U != finish_cnt)
        {
            p_node_highest = HANLDER_LINK_LIST(s_wt588_handler_inst)->\
            pf_get_first_node(HANLDER_LINK_LIST(s_wt588_handler_inst));
            if (NULL != p_node_highest)
            {
                HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_send(\
                                          s_wt588_handler_inst.executor_queue,
                                                              &p_node_highest,
                                                               OSAL_MAX_DELAY);
            }
        }

        // if the list is empty, reset current and highest priority
        // to lowest level (15)
        bool is_empty = HANLDER_LINK_LIST(s_wt588_handler_inst)->\
        pf_list_is_empty(HANLDER_LINK_LIST(s_wt588_handler_inst));
        if (is_empty)
        {
            DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "play request list is empty");
            HANLDER_LINK_LIST(s_wt588_handler_inst)->current_priority = 15;
            HANLDER_LINK_LIST(s_wt588_handler_inst)->highest_priority = 15;
        }

        uint8_t cur_pri  = HANLDER_LINK_LIST(s_wt588_handler_inst)->\
                                                              current_priority;
        uint8_t high_pri = HANLDER_LINK_LIST(s_wt588_handler_inst)->\
                                                              highest_priority;
        if ((cur_pri != s_prev_cur_pri) || (high_pri != s_prev_high_pri))
        {
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "priority changed, current: %u->%u, highest: %u->%u",
                      s_prev_cur_pri, cur_pri, s_prev_high_pri, high_pri);
            s_prev_cur_pri  = cur_pri;
            s_prev_high_pri = high_pri;
        }

        HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
            ->p_os_delay_interface->pf_os_delay_ms(300);
    }
}

/**
 * @brief WT588 executor thread
 *
 * Handles voice playback execution, including volume setting, preemption
 * detection, and retry logic for failed play attempts.
 *
 * @param[in] args : Thread arguments (unused)
 *
 * @return void
 *
 * */
static void wt588_executor_thread(void *const args)
{
    DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "wt588_executor_thread running");

    list_voice_node_t *p_node_exec = NULL;
    uint8_t            fail_cnt    = 0;
    bool               busy_found  = false;

    while (1)
    {
        HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->\
        pf_os_queue_get(s_wt588_handler_inst.executor_queue,
                                                        &p_node_exec,
                                                     OSAL_MAX_DELAY);

        // NULL sentinel: executor was idle (blocking on queue_get) when stop fired
        if (NULL == p_node_exec)
        {
            s_wt588_handler_inst.p_wt588_driver->pf_stop_play(
                s_wt588_handler_inst.p_wt588_driver);
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_sema_give(
                s_wt588_handler_inst.stop_ack_semaphore);
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG, "stop sentinel: 0xFE sent, ack given");
            continue;
        }

        // Stop flag set: executor holds a node pointer — abort before touching it
        if (s_stop_requested)
        {
            s_stop_requested = false;
            s_wt588_handler_inst.p_wt588_driver->pf_stop_play(
                s_wt588_handler_inst.p_wt588_driver);
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_sema_give(
                s_wt588_handler_inst.stop_ack_semaphore);
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "stop flag on dequeue: 0xFE sent, node discarded pri=%u",
                      p_node_exec->priority);
            continue;
        }

        fail_cnt = 0;

retry_play:
        // Check stop flag again at every retry entry
        if (s_stop_requested)
        {
            s_stop_requested = false;
            s_wt588_handler_inst.p_wt588_driver->pf_stop_play(
                s_wt588_handler_inst.p_wt588_driver);
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_sema_give(
                s_wt588_handler_inst.stop_ack_semaphore);
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "stop flag at retry: 0xFE sent, node discarded pri=%u",
                      p_node_exec->priority);
            continue;
        }

        s_wt588_handler_inst.p_wt588_driver->pf_set_volume(
            s_wt588_handler_inst.p_wt588_driver, p_node_exec->volume);

        bool is_preempting = s_wt588_handler_inst.p_wt588_driver->pf_is_busy(
                                    s_wt588_handler_inst.p_wt588_driver);
        if (!is_preempting)
        {
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "no busy detected, start playing voice with priority %u",
                      p_node_exec->priority);
        }
        else
        {
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "busy detected, preempting with voice priority %u",
                      p_node_exec->priority);
            // notify busy thread that previous voice was preempted
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
                ->pf_os_sema_give(s_wt588_handler_inst.semaphore_handler);
        }

        s_wt588_handler_inst.p_wt588_driver->pf_start_play(
                s_wt588_handler_inst.p_wt588_driver, p_node_exec->volume_addr);
        DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                 "%s voice addr=0x%02X vol=0x%02X pri=%u",
                 is_preempting ? "preempt" : "start", p_node_exec->volume_addr,
                 p_node_exec->volume, p_node_exec->priority);

        busy_found = false;
        for (uint16_t poll = 0; poll < WT588_BUSY_POLL_CNT; poll++)
        {
            // Abort poll early if stop is requested
            if (s_stop_requested)
            {
                break;
            }

            if (s_wt588_handler_inst.p_wt588_driver->pf_is_busy(
                    s_wt588_handler_inst.p_wt588_driver))
            {
                busy_found = true;
                HANLDER_LINK_LIST(s_wt588_handler_inst)->current_priority =
                    HANLDER_LINK_LIST(s_wt588_handler_inst)->highest_priority;

                HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_send(
                                        s_wt588_handler_inst.busy_detection_queue,
                                                                     &p_node_exec,
                                                                   OSAL_MAX_DELAY);
                DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                       "voice with priority %u is playing, sent to busy detection",
                                                            p_node_exec->priority);
                break;
            }

            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
                ->p_os_delay_interface->pf_os_delay_ms(WT588_BUSY_POLL_MS);
        }

        // Stop flag set during busy poll: send 0xFE and ack handler
        if (s_stop_requested)
        {
            s_stop_requested = false;
            s_wt588_handler_inst.p_wt588_driver->pf_stop_play(
                s_wt588_handler_inst.p_wt588_driver);
            (void)HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_sema_give(
                s_wt588_handler_inst.stop_ack_semaphore);
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "stop flag in poll: 0xFE sent pri=%u", p_node_exec->priority);
            continue;
        }

        if (!busy_found)
        {
            fail_cnt++;
            if (fail_cnt < WT588_MAX_PLAY_RETRY)
            {
                DEBUG_OUT(w, WT588_HANDLER_LOG_TAG,
                          "busy not detected, retry %u/%u (priority=%u)",
                          fail_cnt, WT588_MAX_PLAY_RETRY,
                          p_node_exec->priority);
                HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
                    ->p_os_delay_interface->pf_os_delay_ms(50);
                goto retry_play;
            }
            // max retries exhausted, node kept in list for next dispatch
            DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                      "play failed after %u retries, node kept (priority=%u)",
                      (unsigned int)WT588_MAX_PLAY_RETRY,
                      p_node_exec->priority);
        }
    }
}

/**
 * @brief WT588 busy detection thread
 *
 * Monitors busy signal to detect when voice playback finishes and
 * handles preemption notifications from executor thread.
 *
 * @param[in] args : Thread arguments (unused)
 *
 * @return void
 *
 * */
static void wt588_busy_detection_thread(void *const args)
{
    list_voice_node_t *p_node_busy     = NULL;
    bool               is_busy          = false;
    wt_handler_status_t ret = WT_HANDLER_ERROR;

    DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "wt588_busy_detection_thread running");
    while (1)
    {
        if (NULL == p_node_busy)
        {
            ret = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_get(
                                     s_wt588_handler_inst.busy_detection_queue,
                                     &p_node_busy,
                                     0);
        }

        if (NULL == p_node_busy)
        {
            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)
                ->p_os_delay_interface->pf_os_delay_ms(50);
            continue;
        }

        ret = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_sema_take(
                                       s_wt588_handler_inst.semaphore_handler,
                                                                            0);
        // preempted by higher priority voice, keep pending for next check
        if (WT_HANDLER_OK == ret)
        {
            DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                      "voice with priority %u preempted, keep pending",
                      p_node_busy->priority);
            p_node_busy = NULL;

            HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->\
                                      p_os_delay_interface->pf_os_delay_ms(50);
            continue;
        }

        is_busy = s_wt588_handler_inst.p_wt588_driver->pf_is_busy(
                                          s_wt588_handler_inst.p_wt588_driver);
        if (!is_busy)
        {
            ret = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->pf_os_queue_send(
                                      s_wt588_handler_inst.voice_finish_queue,
                                                                 &p_node_busy,
                                                               OSAL_MAX_DELAY);
            if (WT_HANDLER_OK == ret)
            {
                DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
                          "voice with priority %u finished playing, sent "
                          "to finish queue",
                          p_node_busy->priority);
                p_node_busy = NULL;
            }
            else
            {
                DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                          "failed to send finish queue");
            }
        }
        else
        {
            DEBUG_OUT(i, WT588_HANDLER_LOG_TAG,
                      "wait for busy to finish for priority %u",
                      p_node_busy->priority);
        }

        HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->\
                            p_os_delay_interface->pf_os_delay_ms(300);
    }
}

/**
 * @brief Initialize WT588 handler resources
 *
 * Creates queues, semaphores, and worker threads for voice playback management.
 * Implements cleanup on failure to ensure resource consistency.
 *
 * @param[in] p_handler_instance : Pointer to handler instance
 *
 * @return wt_handler_status_t WT_HANDLER_OK if successful, error code otherwise
 *
 * */
static wt_handler_status_t wt588_handler_init(bsp_wt588_handler_t *const \
                                                            p_handler_instance)
{
    wt_handler_status_t  ret = WT_HANDLER_OK; 
    wt588_status_t driverRet =   WT588_ERROR;
    list_status_t    linkret =       LIST_OK;
    /************ 1.Checking input parameters ************/
    /************* 2.Checking the Resources **************/
    // has been checked in wt588_handler_inst
    driverRet = wt588_driver_inst(
            p_handler_instance->p_wt588_driver,
            (wt_sys_interface_t *)p_handler_instance->handler_input_args->
                                      p_os_interface->p_os_delay_interface,
            p_handler_instance->handler_input_args->p_busy_interface,
            p_handler_instance->handler_input_args->p_gpio_interface,
            p_handler_instance->handler_input_args->p_pwm_dma_interface);
    ret = wt588_status_to_handler_status(driverRet);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init driver instance failed");
        goto exit;
    }

    /*************** 3. Creating Resources ***************/
    list_malloc_interface_t *p_list_malloc = \
    (list_malloc_interface_t *)HANDLER_OS_INTERFACE(p_handler_instance)->\
                                                           p_os_heap_interface;
    linkret = list_handler_construct(p_handler_instance->p_play_request_list,
                                     p_list_malloc);
    if (LIST_OK != linkret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init list handler construct failed");
        ret = WT_HANDLER_ERROR;
        goto exit;
    }

    DEBUG_OUT(i, WT588_HANDLER_LOG_TAG,
              "wt588_handler_init resource create started");
    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_sema_create(
            &p_handler_instance->semaphore_handler);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init semaphore create failed");
        goto exit;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_sema_create(
            &p_handler_instance->stop_semaphore);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init stop semaphore create failed");
        goto cleanup_semaphore_handler;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_sema_create(
            &p_handler_instance->stop_ack_semaphore);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init stop ack semaphore create failed");
        goto cleanup_stop_semaphore;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_create(
        &p_handler_instance->voice_add_queue, 10, sizeof(list_voice_node_t *));
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init handler queue create failed");
        goto cleanup_stop_ack_semaphore;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_create(
         &p_handler_instance->executor_queue, 10, sizeof(list_voice_node_t *));
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init executor queue create failed");
        goto cleanup_handler_queue;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_create(
            &p_handler_instance->busy_detection_queue,
            10,
            sizeof(list_voice_node_t *));
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init busy detection queue create failed");
        goto cleanup_executor_queue;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_create(
            &p_handler_instance->voice_finish_queue,
            10,
            sizeof(list_voice_node_t *));
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "wt588_handler_init voice finish queue create failed");
        goto cleanup_busy_detection_queue;
    }
    DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
              "wt588_handler_init queue setup successful");

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_task_create(
            &p_handler_instance->executor_thread_handle,
            NULL,
            wt588_executor_thread,
            "wt588_executor_thread",
            WT588_EXECUTOR_STACK_DEPTH,
            WT588_EXECUTOR_PRIORITY);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                           "wt588_handler_init executor thread create failed");
        goto cleanup_voice_finish_queue;
    }

    ret = HANDLER_OS_INTERFACE(p_handler_instance)->pf_task_create(
            &p_handler_instance->busy_detection_thread_handle,
            NULL,
            wt588_busy_detection_thread,
            "wt588_busy_detection_thread",
            WT588_BUSY_DET_STACK_DEPTH,
            WT588_BUSY_DET_PRIORITY);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                     "wt588_handler_init busy detection thread create failed");
        goto cleanup_executor_thread;
    }
    DEBUG_OUT(d, WT588_HANDLER_LOG_TAG,
              "wt588_handler_init resources create successful");

    DEBUG_OUT(d, WT588_HANDLER_LOG_TAG, "wt588_handler_init successful");

    goto exit;

cleanup_executor_thread:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_task_delete(
            p_handler_instance->executor_thread_handle);
    p_handler_instance->executor_thread_handle = NULL;

cleanup_voice_finish_queue:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_delete(
            p_handler_instance->voice_finish_queue);
    p_handler_instance->voice_finish_queue = NULL;

cleanup_busy_detection_queue:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_delete(
        p_handler_instance->busy_detection_queue);
    p_handler_instance->busy_detection_queue = NULL;

cleanup_executor_queue:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_delete(
            p_handler_instance->executor_queue);
    p_handler_instance->executor_queue = NULL;

cleanup_handler_queue:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_queue_delete(
            p_handler_instance->voice_add_queue);
    p_handler_instance->voice_add_queue = NULL;

cleanup_stop_ack_semaphore:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_sema_delete(
            p_handler_instance->stop_ack_semaphore);
    p_handler_instance->stop_ack_semaphore = NULL;

cleanup_stop_semaphore:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_sema_delete(
            p_handler_instance->stop_semaphore);
    p_handler_instance->stop_semaphore = NULL;

cleanup_semaphore_handler:
    (void)HANDLER_OS_INTERFACE(p_handler_instance)->pf_os_sema_delete(
            p_handler_instance->semaphore_handler);

exit:
    return ret;
}

/**
 * @brief Create WT588 handler instance
 *
 * Validates input parameters and resources, mounts interfaces, and
 * initializes handler subsystems.
 *
 * @param[in] p_handler_instance   : Pointer to handler instance
 * @param[in] p_handler_input_args : Pointer to handler input arguments
 *
 * @return wt_handler_status_t WT_HANDLER_OK if successful, error code otherwise
 *
 * */
wt_handler_status_t wt588_handler_inst(
                             bsp_wt588_handler_t  * const   p_handler_instance,
                         wt_handler_input_args_t  * const p_handler_input_args)
{
    DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "wt588_handler_inst start");
    wt_handler_status_t ret = WT_HANDLER_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == p_handler_instance                       || 
        NULL == p_handler_input_args)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                 "wt588 handler inst input error parameter");
        ret = WT_HANDLER_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == p_handler_instance->p_wt588_driver)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                 "wt588 handler inst input error resource 0");
        ret = WT_HANDLER_ERRORRESOURCE;
        return ret;
    }
    if (NULL == p_handler_input_args->list_malloc_interface                  ||
        NULL == p_handler_input_args->p_busy_interface                       ||
        NULL == p_handler_input_args->p_gpio_interface                       ||
        NULL == p_handler_input_args->p_os_interface                         ||
        NULL == p_handler_input_args->p_os_interface->p_os_delay_interface   ||
        NULL == p_handler_input_args->p_os_interface->p_os_heap_interface    ||
        NULL == p_handler_input_args->p_pwm_dma_interface)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                 "wt588 handler inst input error resource 1");
        ret = WT_HANDLER_ERRORRESOURCE;
        return ret;
    }
    if (
        NULL == p_handler_input_args->list_malloc_interface->pf_list_malloc  ||
        NULL == p_handler_input_args->list_malloc_interface->pf_list_free    ||
        NULL == p_handler_input_args->p_busy_interface->pf_is_busy           ||
        NULL == p_handler_input_args->p_gpio_interface->pf_wt_gpio_init      ||
        NULL == p_handler_input_args->p_gpio_interface->pf_wt_gpio_deinit    ||
        NULL == p_handler_input_args->p_os_interface->pf_task_create         ||
        NULL == p_handler_input_args->p_os_interface->pf_task_delete         ||
        NULL == p_handler_input_args->p_os_interface->pf_os_queue_create     ||
        NULL == p_handler_input_args->p_os_interface->pf_os_queue_delete     ||
        NULL == p_handler_input_args->p_os_interface->pf_os_queue_send       ||
        NULL == p_handler_input_args->p_os_interface->pf_os_queue_get        ||
        NULL == p_handler_input_args->p_os_interface->pf_os_queue_count      ||        
        NULL == p_handler_input_args->p_os_interface->p_os_delay_interface->\
                                                           pf_os_delay_ms    ||
        NULL == p_handler_input_args->p_os_interface->p_os_heap_interface->\
                                                           pf_os_malloc      ||
        NULL == p_handler_input_args->p_os_interface->p_os_heap_interface->\
                                                           pf_os_free        ||
        NULL == p_handler_input_args->p_pwm_dma_interface->pf_pwm_dma_init   ||
        NULL == p_handler_input_args->p_pwm_dma_interface->pf_pwm_dma_deinit ||
        NULL == p_handler_input_args->p_pwm_dma_interface->pf_pwm_dma_send_byte
        )
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                 "wt588 handler inst input error resource function");
        ret = WT_HANDLER_ERRORRESOURCE;
        return ret;
    }

    /**************** 3.Mount interfaces *****************/
    p_handler_instance->handler_input_args = p_handler_input_args;

    ret = wt588_handler_init(p_handler_instance);
    if (WT_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                 "wt588 handler inst init failed");
        return ret;
    }

    DEBUG_OUT(d, WT588_HANDLER_LOG_TAG, "wt588_handler_inst successful");

    return ret;
}

/**
 * @brief Submit a voice play request
 *
 * Allocates a play request node and queues it for processing by handler thread.
 * Validates priority range and handler initialization state.
 *
 * @param[in] volume_addr : Voice address to play
 * @param[in] volume      : Volume level (0-31)
 * @param[in] priority    : Playback priority (0-14, lower is higher priority)
 *
 * @return wt_handler_status_t WT_HANDLER_OK if queued successfully, error code otherwise
 *
 * */
wt_handler_status_t wt588_handler_play_request(uint8_t volume_addr,
                                               uint8_t volume,
                                               uint8_t priority)
{
    wt_handler_status_t ret = WT_HANDLER_OK;

    if (NULL == s_wt588_handler_inst.voice_add_queue)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "play_request: handler not initialised");
        return WT_HANDLER_ERRORRESOURCE;
    }
    if (priority >= WT588_MAX_PRIORITY)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "play_request: invalid priority %u", priority);
        return WT_HANDLER_ERRORPARAMETER;
    }

    wt_os_heap_interface_t *p_heap =
              HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->p_os_heap_interface;

    list_voice_node_t *p_node =
          (list_voice_node_t *)p_heap->pf_os_malloc(sizeof(list_voice_node_t));
    if (NULL == p_node)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "play_request: malloc failed");
        return WT_HANDLER_ERRORNOMEMORY;
    }

    p_node->volume_addr = volume_addr;
    p_node->volume      = volume;
    p_node->priority    = priority;
    p_node->next        = NULL;
    p_node->prev        = NULL;

    ret = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->
          pf_os_queue_send(s_wt588_handler_inst.voice_add_queue,
                           &p_node, OSAL_MAX_DELAY);
    if (WT_HANDLER_OK != ret)
    {
        p_heap->pf_os_free(p_node);
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "play_request: queue send failed");
    }
    else
    {
        DEBUG_OUT(i, WT588_HANDLER_LOG_TAG,
                  "play_request: addr=0x%02X vol=0x%02X pri=%u queued",
                  volume_addr, volume, priority);
    }

    return ret;
}

/**
 * @brief Request stop of all audio playback
 *
 * Signals handler thread to clear all pending voices and sends 0xFE
 * stop command to hardware via executor thread.
 *
 * @return wt_handler_status_t WT_HANDLER_OK if signal queued successfully
 *
 * */
wt_handler_status_t wt588_handler_stop(void)
{
    if (NULL == s_wt588_handler_inst.stop_semaphore)
    {
        DEBUG_OUT(e, WT588_HANDLER_ERR_LOG_TAG,
                  "handler_stop: handler not initialised");
        return WT_HANDLER_ERRORRESOURCE;
    }

    // Set flag first so executor sees it immediately on next check
    s_stop_requested = true;

    wt_handler_status_t ret = HANDLER_OS_INTERFACE(&s_wt588_handler_inst)->
                              pf_os_sema_give(s_wt588_handler_inst.stop_semaphore);
    DEBUG_OUT(i, WT588_HANDLER_LOG_TAG, "handler_stop: stop signal sent");
    return ret;
}
//******************************* Functions *********************************//
