/******************************************************************************
 * @file bsp_adapter_port_externflash.c
 *
 * @par dependencies
 * - bsp_w25q64_handler.h
 * - bsp_adapter_port_externflash.h
 * - osal_mutex.h
 *
 * @author Ethan-Hang
 *
 * @brief W25Q64 adapter implementation for the externflash wrapper layer.
 *        Translates each externflash_drv_* call into a flash_event_t and
 *        submits it through handler_flash_event_send() of the W25Q64 handler.
 *
 * Processing flow:
 *   externflash_drv_<op>(args, user_cb, user_ctx)
 *     → allocate a trampoline slot                (carries user_cb / user_ctx)
 *     → build flash_event_t (event_type, addr, size, data)
 *         pf_callback = adapter_trampoline
 *         p_user_ctx  = &slot
 *     → handler_flash_event_send(&event)          (queue copies the struct)
 *   handler thread later:
 *     → invokes adapter_trampoline(&event)
 *     → trampoline translates flash_event_t → wp_externflash_result_t
 *     → invokes user_cb, then frees the slot
 *
 * @version V1.0 2026-05-02
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_adapter_port_externflash.h"
#include "bsp_w25q64_handler.h"

#include "osal_mutex.h"
#include "osal_common_types.h"
#include "osal_error.h"

#include <string.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define EXTERNFLASH_TRAMPOLINE_SLOT_NUM      (10U)

#define EXTERNFLASH_W25Q64_DEV_ID            (0x00EF4017U)
//******************************** Defines **********************************//

//******************************* Variables *********************************//
typedef struct
{
    bool                         in_use;
    wp_externflash_callback_t   user_cb;
    void                    *  user_ctx;
} trampoline_slot_t;

static trampoline_slot_t      s_slot_pool[EXTERNFLASH_TRAMPOLINE_SLOT_NUM] = {0};
static osal_mutex_handle_t    s_pool_mutex                                 = NULL;
//******************************* Variables *********************************//

//******************************* Functions *********************************//

/**
 * @brief  Acquire a free trampoline slot from the static pool.
 *
 * @return Pointer to an idle slot or NULL if all are busy.
 */
static trampoline_slot_t *adapter_slot_alloc(void)
{
    trampoline_slot_t *p_slot = NULL;

    if (NULL != s_pool_mutex)
    {
        (void)osal_mutex_take(s_pool_mutex, OSAL_MAX_DELAY);
    }

    for (uint32_t i = 0U; i < EXTERNFLASH_TRAMPOLINE_SLOT_NUM; ++i)
    {
        if (false == s_slot_pool[i].in_use)
        {
            s_slot_pool[i].in_use = true;
            p_slot = &s_slot_pool[i];
            break;
        }
    }

    if (NULL != s_pool_mutex)
    {
        (void)osal_mutex_give(s_pool_mutex);
    }

    return p_slot;
}

/**
 * @brief  Return a trampoline slot to the pool.
 */
static void adapter_slot_free(trampoline_slot_t *p_slot)
{
    if (NULL == p_slot)
    {
        return;
    }

    if (NULL != s_pool_mutex)
    {
        (void)osal_mutex_take(s_pool_mutex, OSAL_MAX_DELAY);
    }

    p_slot->user_cb = NULL;
    p_slot->user_ctx = NULL;
    p_slot->in_use = false;

    if (NULL != s_pool_mutex)
    {
        (void)osal_mutex_give(s_pool_mutex);
    }
}

/**
 * @brief  Map a flash_handler_status_t onto wp_externflash_status_t.
 *
 * Both enums share values 0..7 (OK / ERROR / TIMEOUT / RESOURCE / PARAMETER /
 * NOMEMORY / UNSUPPORTED / ISR), so a direct cast is safe.
 */
static wp_externflash_status_t adapter_status_translate(
                                            flash_handler_status_t hdl_status)
{
    return (wp_externflash_status_t)hdl_status;
}

/**
 * @brief  Trampoline invoked by the handler thread once the queued event
 *         finishes.  Re-packages the flash_event_t into a wrapper-level
 *         result and forwards to the user callback.
 *
 * @param[in] args : Pointer to flash_event_t handed in by the handler.
 */
static void adapter_trampoline(void *args)
{
    flash_event_t *p_evt = (flash_event_t *)args;
    if (NULL == p_evt)
    {
        return;
    }

    trampoline_slot_t *p_slot = (trampoline_slot_t *)p_evt->p_user_ctx;
    if (NULL == p_slot)
    {
        return;
    }

    wp_externflash_result_t result = {
        .addr       =                           p_evt->addr,
        .size       =                           p_evt->size,
        .data       =                           p_evt->data,
        .status     = adapter_status_translate(p_evt->status),
        .p_user_ctx =                       p_slot->user_ctx,
    };

    wp_externflash_callback_t user_cb = p_slot->user_cb;

    /* Free the slot before invoking the user CB so the caller is free to
     * issue another asynchronous request from inside its callback. */
    adapter_slot_free(p_slot);

    if (NULL != user_cb)
    {
        user_cb(&result);
    }
}

/**
 * @brief  Common path: package the event, attach the trampoline, and submit.
 */
static wp_externflash_status_t adapter_submit_event(
                                  flash_handler_event_type_t  event_type,
                                  uint32_t                          addr,
                                  uint8_t  *                        data,
                                  uint32_t                          size,
                                  wp_externflash_callback_t      user_cb,
                                  void     *                    user_ctx)
{
    trampoline_slot_t *p_slot = adapter_slot_alloc();
    if (NULL == p_slot)
    {
        return WP_EXTERNFLASH_ERRORNOMEMORY;
    }

    p_slot->user_cb  = user_cb;
    p_slot->user_ctx = user_ctx;

    flash_event_t evt = {
        .addr        =              addr,
        .size        =              size,
        .data        =              data,
        .status      =  FLASH_HANLDER_OK,
        .event_type  =        event_type,
        .p_user_ctx  =            p_slot,
        .pf_callback = adapter_trampoline,
    };

    flash_handler_status_t ret = handler_flash_event_send(&evt);
    if (FLASH_HANLDER_OK != ret)
    {
        adapter_slot_free(p_slot);
        return adapter_status_translate(ret);
    }

    return WP_EXTERNFLASH_OK;
}

/* ---- Vtable functions --------------------------------------------------- */

static void w25q64_externflash_drv_init(externflash_drv_t *const dev)
{
    (void)dev;
    /* Hardware init is performed inside flash_handler_thread() at task
     * entry; no extra action required here. */
}

static void w25q64_externflash_drv_deinit(externflash_drv_t *const dev)
{
    (void)dev;
}

static wp_externflash_status_t w25q64_externflash_read(
                                            externflash_drv_t *const   dev,
                                                          uint32_t    addr,
                                                          uint8_t *   data,
                                                          uint32_t    size,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    if (NULL == data || 0U == size)
    {
        return WP_EXTERNFLASH_ERRORPARAMETER;
    }
    return adapter_submit_event(FLASH_HANDLER_EVENT_READ,
                                addr, data, size, cb, p_user_ctx);
}

static wp_externflash_status_t w25q64_externflash_write(
                                            externflash_drv_t *const   dev,
                                                          uint32_t    addr,
                                                          uint8_t *   data,
                                                          uint32_t    size,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    if (NULL == data || 0U == size)
    {
        return WP_EXTERNFLASH_ERRORPARAMETER;
    }
    return adapter_submit_event(FLASH_HANDLER_EVENT_WRITE,
                                addr, data, size, cb, p_user_ctx);
}

static wp_externflash_status_t w25q64_externflash_write_noerase(
                                            externflash_drv_t *const   dev,
                                                          uint32_t    addr,
                                                          uint8_t *   data,
                                                          uint32_t    size,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    if (NULL == data || 0U == size)
    {
        return WP_EXTERNFLASH_ERRORPARAMETER;
    }
    return adapter_submit_event(FLASH_HANDLER_EVENT_WRITE_NOERASE,
                                addr, data, size, cb, p_user_ctx);
}

static wp_externflash_status_t w25q64_externflash_erase_chip(
                                            externflash_drv_t *const   dev,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    return adapter_submit_event(FLASH_HANDLER_EVENT_ERASE_CHIP,
                                0U, NULL, 0U, cb, p_user_ctx);
}

static wp_externflash_status_t w25q64_externflash_erase_sector(
                                            externflash_drv_t *const   dev,
                                                          uint32_t    addr,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    return adapter_submit_event(FLASH_HANDLER_EVENT_ERASE_SECTOR,
                                addr, NULL, 0U, cb, p_user_ctx);
}

static wp_externflash_status_t w25q64_externflash_sleep(
                                            externflash_drv_t *const   dev,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    return adapter_submit_event(FLASH_HANDLER_EVENT_SLEEP,
                                0U, NULL, 0U, cb, p_user_ctx);
}

static wp_externflash_status_t w25q64_externflash_wakeup(
                                            externflash_drv_t *const   dev,
                                            wp_externflash_callback_t   cb,
                                                          void *p_user_ctx)
{
    (void)dev;
    return adapter_submit_event(FLASH_HANDLER_EVENT_WAKEUP,
                                0U, NULL, 0U, cb, p_user_ctx);
}

/**
 * @brief   Register the W25Q64 driver into the externflash wrapper.
 *
 *          The mutex protecting the trampoline slot pool is created here so
 *          that the wrapper APIs are immediately safe to call from any task
 *          once the kernel is running.  Object creation only allocates from
 *          the FreeRTOS heap; no scheduler activity is required, so it is
 *          safe to call from the pre-kernel registration phase.
 *
 * @return  true  - Registration successful.
 *          false - Mount failed.
 */
bool drv_adapter_externflash_register(void)
{
    if (NULL == s_pool_mutex)
    {
        if (OSAL_SUCCESS != osal_mutex_init(&s_pool_mutex))
        {
            s_pool_mutex = NULL;
        }
    }

    externflash_drv_t externflash_drv = {
        .idx                            =                                 0,
        .dev_id                         =        EXTERNFLASH_W25Q64_DEV_ID,
        .user_data                      =                              NULL,
        .pf_externflash_drv_init        =      w25q64_externflash_drv_init,
        .pf_externflash_drv_deinit      =    w25q64_externflash_drv_deinit,
        .pf_externflash_read            =          w25q64_externflash_read,
        .pf_externflash_write           =         w25q64_externflash_write,
        .pf_externflash_write_noerase   = w25q64_externflash_write_noerase,
        .pf_externflash_erase_chip      =    w25q64_externflash_erase_chip,
        .pf_externflash_erase_sector    =  w25q64_externflash_erase_sector,
        .pf_externflash_sleep           =         w25q64_externflash_sleep,
        .pf_externflash_wakeup          =        w25q64_externflash_wakeup,
    };

    return drv_adapter_externflash_mount(0, &externflash_drv);
}

//******************************* Functions *********************************//
