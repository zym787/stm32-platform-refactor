/******************************************************************************
 * @file storage_manager_task.c
 *
 * @par dependencies
 * - service_storage_facade.h
 * - bsp_wrapper_externflash.h
 * - osal_event_group.h / osal_sema.h / osal_mutex.h
 *
 * @author Ethan-Hang
 *
 * @brief Service-layer storage manager.  Provides blocking Read_LvglData /
 *        Write_LvglData (and Read_/Write_OtaData) APIs to other tasks while
 *        internally driving the externflash wrapper through its async +
 *        callback API. Lives under 01_SERVICE/service_storage/ to keep the
 *        layering clean: APP (LVGL) and 01_SERVICE/service_ota share this
 *        facade rather than reaching into one another.
 *
 * Processing flow:
 *   - Caller takes s_caller_mutex, publishes the request into s_pending,
 *     sets a bit in s_evgrp, and blocks on s_done_sem.
 *   - storage_manager_task wakes on the event-group bit, reads s_pending,
 *     submits to the externflash wrapper with on_done_cb as the completion
 *     callback, and goes back to wait on the event group.
 *   - on_done_cb (handler-thread context) records the completion status
 *     into s_pending.last_status and gives s_done_sem to wake the caller.
 *
 *   The mutex held across the entire round-trip serialises callers and
 *   keeps s_pending stable for the manager to read.
 *
 * @version V1.0 2026-05-08
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "service_storage_facade.h"

#include "bsp_wrapper_externflash.h"
#include "osal_event_group.h"
#include "osal_sema.h"
#include "osal_mutex.h"
#include "osal_task.h"
#include "osal_error.h"

#include "Debug.h"

#include <stddef.h>
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define STORAGE_EVENT_MASK_ALL    (EVENT_OTA      | \
                                   EVENT_FLASHDB  | \
                                   EVENT_FATFS    | \
                                   EVENT_LVGL_R   | \
                                   EVENT_LVGL_W   | \
                                   EVENT_CALIB_R  | \
                                   EVENT_CALIB_W)
//******************************** Defines **********************************//

//******************************** Typedefs *********************************//
typedef enum
{
    STORAGE_OP_NONE = 0,
    STORAGE_OP_LVGL_READ,
    STORAGE_OP_LVGL_WRITE,
    STORAGE_OP_OTA_READ,
    STORAGE_OP_OTA_WRITE,
    STORAGE_OP_CALIB_READ,
    STORAGE_OP_CALIB_WRITE,
} storage_op_t;

typedef struct
{
    uint32_t                addr;        /* offset within LVGL sub-region   */
    uint32_t                size;
    uint8_t                *buf;         /* read: dst, write: src           */
    storage_op_t              op;
    platform_err_t   last_status;
} storage_request_t;
//******************************** Typedefs *********************************//

//******************************* Variables *********************************//
static osal_event_group_handle_t  s_evgrp        = NULL;
static osal_sema_handle_t         s_done_sem     = NULL;
static osal_mutex_handle_t        s_caller_mutex = NULL;

static storage_request_t          s_pending      = {0};
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
 * @brief Async completion callback invoked by the externflash adapter
 *        trampoline (handler-thread context, never ISR).
 */
static void on_done_cb(wp_externflash_result_t *result)
{
    s_pending.last_status = (NULL != result) ? result->status
                                             : PLATFORM_ERR_GENERAL;
    if (NULL != s_done_sem)
    {
        (void)osal_sema_give(s_done_sem);
    }
}

/**
 * @brief Translate the wrapper status into the APP-layer status enum.
 */
static ext_flash_status_t translate_status(platform_err_t st)
{
    switch (st)
    {
        case PLATFORM_OK:                 return EXT_FLASH_OK;
        case PLATFORM_ERR_TIMEOUT:       return EXT_FLASH_ERRORTIMEOUT;
        case PLATFORM_ERR_NO_RESOURCE:      return EXT_FLASH_ERRORRESOURCE;
        case PLATFORM_ERR_PARAM:     return EXT_FLASH_ERRORPARAMETER;
        case PLATFORM_ERR_NO_MEMORY:      return EXT_FLASH_ERRORNOMEMORY;
        default:                                return EXT_FLASH_ERROR;
    }
}

/**
 * @brief Common blocking dispatch: parameter check, take mutex, publish
 *        request, signal event, wait for completion.
 */
static ext_flash_status_t lvgl_blocking_dispatch(uint32_t      addr,
                                                 uint32_t      size,
                                                 uint8_t      *buf,
                                                 storage_op_t    op,
                                                 uint32_t   event_bit)
{
    if ((NULL == buf) || (0U == size))
    {
        return EXT_FLASH_ERRORPARAMETER;
    }

    if (size > MEMORY_LVGL_REGION_SIZE)
    {
        return EXT_FLASH_ERRORNOMEMORY;
    }

    if ((addr + size) > MEMORY_LVGL_REGION_SIZE)
    {
        return EXT_FLASH_ERRORNOMEMORY;
    }

    if ((NULL == s_caller_mutex) ||
        (NULL == s_evgrp)        ||
        (NULL == s_done_sem))
    {
        return EXT_FLASH_ERRORRESOURCE;
    }

    if (OSAL_SUCCESS != osal_mutex_take(s_caller_mutex, OSAL_MAX_DELAY))
    {
        return EXT_FLASH_ERROR;
    }

    s_pending.addr        = addr;
    s_pending.size        = size;
    s_pending.buf         = buf;
    s_pending.op          = op;
    s_pending.last_status = PLATFORM_ERR_GENERAL;

    (void)osal_event_group_set_bits(s_evgrp, event_bit);

    /**
     * Wait forever for the manager / handler / callback chain to give the
     * sema.  This is a blocking call by design -- callers needing
     * non-blocking access should use the externflash wrapper directly.
     */
    (void)osal_sema_take(s_done_sem, OSAL_MAX_DELAY);

    ext_flash_status_t ret = translate_status(s_pending.last_status);

    (void)osal_mutex_give(s_caller_mutex);
    return ret;
}

ext_flash_status_t storage_manager_resources_init(void)
{
    if (NULL == s_evgrp)
    {
        if (OSAL_SUCCESS != osal_event_group_create(&s_evgrp))
        {
            return EXT_FLASH_ERRORRESOURCE;
        }
    }

    if (NULL == s_done_sem)
    {
        /* Binary semaphore starts empty so the first take() always blocks
         * until on_done_cb gives. */
        if (OSAL_SUCCESS != osal_sema_init(&s_done_sem, 0U))
        {
            return EXT_FLASH_ERRORRESOURCE;
        }
    }

    if (NULL == s_caller_mutex)
    {
        if (OSAL_SUCCESS != osal_mutex_init(&s_caller_mutex))
        {
            return EXT_FLASH_ERRORRESOURCE;
        }
    }

    return EXT_FLASH_OK;
}

ext_flash_status_t Read_LvglData(uint32_t  addr,
                                 uint32_t  size,
                                 uint8_t  *out_buf)
{
    return lvgl_blocking_dispatch(addr, size, out_buf,
                                  STORAGE_OP_LVGL_READ, EVENT_LVGL_R);
}

ext_flash_status_t Write_LvglData(uint32_t        addr,
                                  uint32_t        size,
                                  const uint8_t  *in_buf)
{
    /* The wrapper's pf_externflash_write needs a non-const pointer; the
     * underlying SPI write path only reads from this buffer, and the API
     * is blocking, so casting away const is safe here. */
    return lvgl_blocking_dispatch(addr, size, (uint8_t *)in_buf,
                                  STORAGE_OP_LVGL_WRITE, EVENT_LVGL_W);
}

/**
 * @brief OTA-region twin of lvgl_blocking_dispatch — same protocol but the
 *        range check uses the OTA sub-region size and the physical address
 *        computed in storage_manager_task uses MEMORY_OTA_START_ADDRESS.
 */
static ext_flash_status_t ota_blocking_dispatch(uint32_t      addr,
                                                uint32_t      size,
                                                uint8_t      *buf,
                                                storage_op_t  op)
{
    if ((NULL == buf) || (0U == size))
    {
        return EXT_FLASH_ERRORPARAMETER;
    }

    if ((size > MEMORY_OTA_REGION_SIZE) ||
        ((addr + size) > MEMORY_OTA_REGION_SIZE))
    {
        return EXT_FLASH_ERRORNOMEMORY;
    }

    if ((NULL == s_caller_mutex) ||
        (NULL == s_evgrp)        ||
        (NULL == s_done_sem))
    {
        return EXT_FLASH_ERRORRESOURCE;
    }

    if (OSAL_SUCCESS != osal_mutex_take(s_caller_mutex, OSAL_MAX_DELAY))
    {
        return EXT_FLASH_ERROR;
    }

    s_pending.addr        = addr;
    s_pending.size        = size;
    s_pending.buf         = buf;
    s_pending.op          = op;
    s_pending.last_status = PLATFORM_ERR_GENERAL;

    (void)osal_event_group_set_bits(s_evgrp, EVENT_OTA);

    (void)osal_sema_take(s_done_sem, OSAL_MAX_DELAY);

    ext_flash_status_t ret = translate_status(s_pending.last_status);

    (void)osal_mutex_give(s_caller_mutex);
    return ret;
}

ext_flash_status_t Read_OtaData(uint32_t  addr,
                                uint32_t  size,
                                uint8_t  *out_buf)
{
    return ota_blocking_dispatch(addr, size, out_buf, STORAGE_OP_OTA_READ);
}

ext_flash_status_t Write_OtaData(uint32_t        addr,
                                 uint32_t        size,
                                 const uint8_t  *in_buf)
{
    /* externflash_drv_write needs a non-const pointer; SPI path only reads
       from the buffer, and the wrapper is blocking, so casting is safe. */
    return ota_blocking_dispatch(addr, size, (uint8_t *)in_buf,
                                 STORAGE_OP_OTA_WRITE);
}

/**
 * @brief Calibration-region twin of lvgl_blocking_dispatch.
 *
 *        Identical handshake (mutex, request publish, event-bit, wait sem)
 *        but range-checks against MEMORY_CALIB_REGION_SIZE so a caller
 *        cannot accidentally write past the 4-KB block.  storage_manager_task
 *        rebases s_pending.addr to MEMORY_CALIB_START_ADDRESS at dispatch.
 */
static ext_flash_status_t calib_blocking_dispatch(uint32_t      addr,
                                                  uint32_t      size,
                                                  uint8_t      *buf,
                                                  storage_op_t  op,
                                                  uint32_t  event_bit)
{
    if ((NULL == buf) || (0U == size))
    {
        return EXT_FLASH_ERRORPARAMETER;
    }

    if ((size > MEMORY_CALIB_REGION_SIZE) ||
        ((addr + size) > MEMORY_CALIB_REGION_SIZE))
    {
        return EXT_FLASH_ERRORNOMEMORY;
    }

    if ((NULL == s_caller_mutex) ||
        (NULL == s_evgrp)        ||
        (NULL == s_done_sem))
    {
        return EXT_FLASH_ERRORRESOURCE;
    }

    if (OSAL_SUCCESS != osal_mutex_take(s_caller_mutex, OSAL_MAX_DELAY))
    {
        return EXT_FLASH_ERROR;
    }

    s_pending.addr        = addr;
    s_pending.size        = size;
    s_pending.buf         = buf;
    s_pending.op          = op;
    s_pending.last_status = PLATFORM_ERR_GENERAL;

    (void)osal_event_group_set_bits(s_evgrp, event_bit);

    (void)osal_sema_take(s_done_sem, OSAL_MAX_DELAY);

    ext_flash_status_t ret = translate_status(s_pending.last_status);

    (void)osal_mutex_give(s_caller_mutex);
    return ret;
}

ext_flash_status_t Read_CalibData(uint32_t  addr,
                                  uint32_t  size,
                                  uint8_t  *out_buf)
{
    return calib_blocking_dispatch(addr, size, out_buf,
                                   STORAGE_OP_CALIB_READ, EVENT_CALIB_R);
}

ext_flash_status_t Write_CalibData(uint32_t        addr,
                                   uint32_t        size,
                                   const uint8_t  *in_buf)
{
    /* externflash_drv_write needs a non-const pointer; the underlying SPI
     * write path only reads from this buffer, and the API is blocking, so
     * casting away const is safe here. */
    return calib_blocking_dispatch(addr, size, (uint8_t *)in_buf,
                                   STORAGE_OP_CALIB_WRITE, EVENT_CALIB_W);
}

void storage_manager_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, W25Q64_LOG_TAG, "storage_manager_task entered");

    for (;;)
    {
        uint32_t bits_set = 0U;
        int32_t  wait_ret = osal_event_group_wait_bits(s_evgrp,
                                                       STORAGE_EVENT_MASK_ALL,
                                                       true,    /* clear   */
                                                       false,   /* any bit */
                                                       OSAL_MAX_DELAY,
                                                       &bits_set);
        if (OSAL_SUCCESS != wait_ret)
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "storage_manager: evgrp wait err=%d", (int)wait_ret);
            continue;
        }

        platform_err_t  st       = PLATFORM_ERR_GENERAL;
        bool                     handled  = false;

        /**
        * Two address bases live on the same W25Q64; pick the base from the
        * event bit set by the caller. EVENT_LVGL_* and EVENT_OTA are
        * mutually exclusive because s_caller_mutex serialises clients.
        **/
        if (0U != (bits_set & EVENT_LVGL_R))
        {
            const uint32_t addr_phy = MEMORY_LVGL_START_ADDRESS +
                                      s_pending.addr;
            st = externflash_drv_read(addr_phy, s_pending.buf, s_pending.size,
                                      on_done_cb, NULL);
            handled = true;
        }
        else if (0U != (bits_set & EVENT_LVGL_W))
        {
            const uint32_t addr_phy = MEMORY_LVGL_START_ADDRESS +
                                      s_pending.addr;
            st = externflash_drv_write(addr_phy, s_pending.buf, s_pending.size,
                                       on_done_cb, NULL);
            handled = true;
        }
        else if (0U != (bits_set & EVENT_CALIB_R))
        {
            const uint32_t addr_phy = MEMORY_CALIB_START_ADDRESS +
                                      s_pending.addr;
            st = externflash_drv_read(addr_phy, s_pending.buf, s_pending.size,
                                      on_done_cb, NULL);
            handled = true;
        }
        else if (0U != (bits_set & EVENT_CALIB_W))
        {
            const uint32_t addr_phy = MEMORY_CALIB_START_ADDRESS +
                                      s_pending.addr;
            st = externflash_drv_write(addr_phy, s_pending.buf, s_pending.size,
                                       on_done_cb, NULL);
            handled = true;
        }
        else if (0U != (bits_set & EVENT_OTA))
        {
            /**
            * OTA path: single event bit, sub-op selects read vs write so
            * we don't burn extra event bits. ERASE not exposed yet —
            * externflash_drv_write auto-erases sectors, which is enough
            * for the upgrade write path.
            **/
            const uint32_t addr_phy = MEMORY_OTA_START_ADDRESS +
                                      s_pending.addr;
            if (STORAGE_OP_OTA_READ == s_pending.op)
            {
                st = externflash_drv_read(addr_phy, s_pending.buf,
                                          s_pending.size, on_done_cb, NULL);
                handled = true;
            }
            else if (STORAGE_OP_OTA_WRITE == s_pending.op)
            {
                st = externflash_drv_write(addr_phy, s_pending.buf,
                                           s_pending.size, on_done_cb, NULL);
                handled = true;
            }
        }
        else
        {
            DEBUG_OUT(w, W25Q64_LOG_TAG,
                      "storage_manager: unhandled bits=0x%08X",
                      (unsigned int)bits_set);
        }

        if (handled && (PLATFORM_OK != st))
        {
            /* Submit failed synchronously -- callback will not fire, so
             * publish the error and unblock the caller ourselves. */
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "storage_manager: submit failed st=%d", (int)st);
            s_pending.last_status = st;
            (void)osal_sema_give(s_done_sem);
        }
    }
}
//******************************* Functions *********************************//
