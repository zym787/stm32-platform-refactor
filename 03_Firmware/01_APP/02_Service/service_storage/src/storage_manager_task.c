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
#include "platform_type.h"
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
    UINT32_T                addr;        /* offset within the sub-region    */
    UINT32_T                size;
    UINT8_T                *buf;         /* read: dst, write: src           */
    storage_op_t              op;
    platform_err_t   last_status;
} storage_request_t;

/**
 * Per-operation descriptor. Each storage_op_t fully determines its
 * sub-region bound, the physical base on the W25Q64, the event-group bit
 * that wakes the manager, and the transfer direction -- so one table
 * replaces the three near-identical *_blocking_dispatch twins and the
 * per-region if/else chain in storage_manager_task.
 */
typedef struct
{
    UINT32_T  region_size;   /* sub-region length, for the bounds check */
    UINT32_T  phys_base;     /* physical base address on the W25Q64     */
    UINT32_T  event_bit;     /* event-group bit that wakes the manager  */
    BOOL_T    is_write;      /* true = program, false = read            */
} storage_op_desc_t;
//******************************** Typedefs *********************************//

//******************************* Variables *********************************//
static osal_event_group_handle_t  s_evgrp        = NULL;
static osal_sema_handle_t         s_done_sem     = NULL;
static osal_mutex_handle_t        s_caller_mutex = NULL;

static storage_request_t          s_pending      = {0};

/* OTA read and write share EVENT_OTA (the op field disambiguates) so the
   manager doesn't burn two event bits on one region. */
static const storage_op_desc_t g_opDesc[] =
{
    [STORAGE_OP_NONE] =
        { 0U, 0U, 0U, false },
    [STORAGE_OP_LVGL_READ] =
        { MEMORY_LVGL_REGION_SIZE, MEMORY_LVGL_START_ADDRESS,
          EVENT_LVGL_R, false },
    [STORAGE_OP_LVGL_WRITE] =
        { MEMORY_LVGL_REGION_SIZE, MEMORY_LVGL_START_ADDRESS,
          EVENT_LVGL_W, true },
    [STORAGE_OP_OTA_READ] =
        { MEMORY_OTA_REGION_SIZE, MEMORY_OTA_START_ADDRESS,
          EVENT_OTA, false },
    [STORAGE_OP_OTA_WRITE] =
        { MEMORY_OTA_REGION_SIZE, MEMORY_OTA_START_ADDRESS,
          EVENT_OTA, true },
    [STORAGE_OP_CALIB_READ] =
        { MEMORY_CALIB_REGION_SIZE, MEMORY_CALIB_START_ADDRESS,
          EVENT_CALIB_R, false },
    [STORAGE_OP_CALIB_WRITE] =
        { MEMORY_CALIB_REGION_SIZE, MEMORY_CALIB_START_ADDRESS,
          EVENT_CALIB_W, true },
};
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
 * @brief Region-agnostic blocking dispatch shared by every Read_/Write_*
 *        facade: validate, take the caller mutex, publish the request,
 *        raise the op's event bit, and block until the completion callback
 *        gives s_done_sem. The op alone selects the sub-region bound and
 *        event bit via g_opDesc, so LVGL / OTA / CALIB share this one path.
 *
 * @param[in]  addr : byte offset INSIDE the op's sub-region.
 * @param[in]  size : transfer length in bytes.
 * @param[in]  buf  : read destination or write source (caller-owned).
 * @param[in]  op   : which sub-region + direction; indexes g_opDesc.
 *
 * @return EXT_FLASH_OK on success, an EXT_FLASH_ERROR* code otherwise.
 */
static ext_flash_status_t blocking_dispatch(UINT32_T      addr,
                                            UINT32_T      size,
                                            UINT8_T      *buf,
                                            storage_op_t  op)
{
    /* Reject an out-of-table op before indexing g_opDesc. */
    if ((op <= STORAGE_OP_NONE) || (op > STORAGE_OP_CALIB_WRITE))
    {
        return EXT_FLASH_ERRORPARAMETER;
    }
    const storage_op_desc_t *desc = &g_opDesc[op];

    if ((NULL == buf) || (0U == size))
    {
        return EXT_FLASH_ERRORPARAMETER;
    }

    /* Bound the request to the op's sub-region (size first so addr + size
       cannot wrap for any in-region offset). */
    if ((size > desc->region_size) ||
        ((addr + size) > desc->region_size))
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

    (void)osal_event_group_set_bits(s_evgrp, desc->event_bit);

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

ext_flash_status_t Read_LvglData(UINT32_T  addr,
                                 UINT32_T  size,
                                 UINT8_T  *out_buf)
{
    return blocking_dispatch(addr, size, out_buf, STORAGE_OP_LVGL_READ);
}

ext_flash_status_t Write_LvglData(UINT32_T        addr,
                                  UINT32_T        size,
                                  const UINT8_T  *in_buf)
{
    /* The wrapper's pf_externflash_write needs a non-const pointer; the
     * underlying SPI write path only reads from this buffer, and the API
     * is blocking, so casting away const is safe here. */
    return blocking_dispatch(addr, size, (UINT8_T *)in_buf,
                             STORAGE_OP_LVGL_WRITE);
}

ext_flash_status_t Read_OtaData(UINT32_T  addr,
                                UINT32_T  size,
                                UINT8_T  *out_buf)
{
    return blocking_dispatch(addr, size, out_buf, STORAGE_OP_OTA_READ);
}

ext_flash_status_t Write_OtaData(UINT32_T        addr,
                                 UINT32_T        size,
                                 const UINT8_T  *in_buf)
{
    /* externflash_drv_write needs a non-const pointer; SPI path only reads
       from the buffer, and the wrapper is blocking, so casting is safe. */
    return blocking_dispatch(addr, size, (UINT8_T *)in_buf,
                             STORAGE_OP_OTA_WRITE);
}

ext_flash_status_t Read_CalibData(UINT32_T  addr,
                                  UINT32_T  size,
                                  UINT8_T  *out_buf)
{
    return blocking_dispatch(addr, size, out_buf, STORAGE_OP_CALIB_READ);
}

ext_flash_status_t Write_CalibData(UINT32_T        addr,
                                   UINT32_T        size,
                                   const UINT8_T  *in_buf)
{
    /* externflash_drv_write needs a non-const pointer; the underlying SPI
     * write path only reads from this buffer, and the API is blocking, so
     * casting away const is safe here. */
    return blocking_dispatch(addr, size, (UINT8_T *)in_buf,
                             STORAGE_OP_CALIB_WRITE);
}

void storage_manager_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, W25Q64_LOG_TAG, "storage_manager_task entered");

    for (;;)
    {
        UINT32_T bits_set = 0U;
        INT32_T  wait_ret = osal_event_group_wait_bits(s_evgrp,
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

        /**
        * The op was published into s_pending under s_caller_mutex together
        * with its event bit, and the mutex serialises callers, so s_pending.op
        * is the single source of truth for region base + direction here. The
        * woken event bit (any of EVENT_*) only signals "a request arrived".
        **/
        const storage_op_t op = s_pending.op;
        if ((op <= STORAGE_OP_NONE) || (op > STORAGE_OP_CALIB_WRITE))
        {
            DEBUG_OUT(w, W25Q64_LOG_TAG,
                      "storage_manager: unhandled op=%d bits=0x%08X",
                      (int)op, (unsigned int)bits_set);
            continue;
        }

        const storage_op_desc_t *desc     = &g_opDesc[op];
        const UINT32_T           addr_phy = desc->phys_base + s_pending.addr;
        platform_err_t           st;

        if (desc->is_write)
        {
            /* externflash_drv_write auto-erases the covered sector(s). */
            st = externflash_drv_write(addr_phy, s_pending.buf,
                                       s_pending.size, on_done_cb, NULL);
        }
        else
        {
            st = externflash_drv_read(addr_phy, s_pending.buf,
                                      s_pending.size, on_done_cb, NULL);
        }

        if (PLATFORM_OK != st)
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
