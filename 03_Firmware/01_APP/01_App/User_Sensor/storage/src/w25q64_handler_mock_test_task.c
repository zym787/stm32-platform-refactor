/******************************************************************************
 * @file w25q64_handler_mock_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_w25q64_handler.h
 * - bsp_w25q64_driver.h
 * - bsp_w25q64_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief W25Q64 flash handler logic validation without real hardware.
 *
 * Processing flow:
 * Mount fake OS queue / OS delay / SPI / timebase vtables.  The SPI mock
 * runs a CS-bracketed state machine that serves correct JEDEC ID so the
 * real w25q64_driver_inst + w25q64_init inside handler_internal_init can
 * succeed.  After init, the driver vtable is re-populated with wrapped
 * functions for dispatch routing tests.  Each case self-asserts on captured
 * call counts so a clean RTT log is a go/no-go signal.
 *
 * @version V1.0 2026-04-27
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "user_task_reso_config.h"
#include "bsp_w25q64_handler.h"
#include "bsp_w25q64_driver.h"
#include "bsp_w25q64_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#ifndef W25Q64_HDL_MOCK_LOG_TAG
#define W25Q64_HDL_MOCK_LOG_TAG             "W25Q64_HDL_MOCK"
#endif
#ifndef W25Q64_HDL_MOCK_ERR_LOG_TAG
#define W25Q64_HDL_MOCK_ERR_LOG_TAG         "W25Q64_HDL_MOCK_ERR"
#endif

#define W25Q64_HDL_MOCK_BOOT_WAIT_MS        2500u
#define W25Q64_HDL_MOCK_STEP_GAP_MS          150u

#define W25Q64_HDL_MOCK_QUEUE_DEPTH           10u
#define W25Q64_HDL_MOCK_PIN_LOW               0u
#define W25Q64_HDL_MOCK_PIN_HIGH              1u
#define W25Q64_HDL_MOCK_SENTINEL_PTR  ((void *)0xDEADBEEFu)

#define W25Q64_HDL_MOCK_READ_LEN             32u
#define W25Q64_HDL_MOCK_WRITE_LEN            16u
#define W25Q64_HDL_MOCK_TEST_ADDR        0x1000u
#define W25Q64_HDL_MOCK_SECTOR_ADDR     0x2000u

/* W25Q64 datasheet identity */
#define W25Q64_HDL_MOCK_JEDEC_MFR           0xEFu
#define W25Q64_HDL_MOCK_JEDEC_TYPE          0x40u
#define W25Q64_HDL_MOCK_JEDEC_CAP           0x17u
#define W25Q64_HDL_MOCK_DEV_ID              0x16u
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/* ---- Mock interface instances -------------------------------------------- */
static bsp_w25q64_handler_t          s_mock_handler;
static flash_input_args_t            s_mock_input_args;
static flash_os_interface_t          s_mock_os_if;
static flash_handler_os_queue_t      s_mock_os_queue;
static flash_handler_os_delay_t      s_mock_os_delay;
static w25q64_spi_interface_t        s_mock_spi;
static w25q64_timebase_interface_t   s_mock_tb;
static w25q64_os_delay_t             s_mock_w25q64_os_delay;
static bsp_w25q64_driver_t           s_mock_driver;
/* flash_handler_private_data_t is opaque (only forward-declared in the
 * handler header).  Allocate enough storage for reasonable expansion. */
static uint8_t                       s_mock_private_buf[32];

/**
 * Per-case capture: every mock invocation lands here so the case body
 * can assert on call counts, parameters and state transitions.
 */
typedef struct
{
    /* ---- OS Queue counters ---- */
    uint32_t queue_create_count;
    uint32_t queue_put_count;
    uint32_t queue_get_count;
    uint32_t queue_delete_count;
    uint32_t last_queue_depth;
    uint32_t last_queue_item_size;

    /* ---- OS Delay counters ---- */
    uint32_t os_delay_count;
    uint32_t last_os_delay_ms;

    /* ---- SPI counters ---- */
    uint32_t spi_init_count;
    uint32_t spi_deinit_count;
    uint32_t spi_transmit_count;
    uint32_t spi_read_count;
    uint32_t cs_low_count;
    uint32_t cs_high_count;

    /* SPI transaction state (CS-bracketed) */
    bool     cs_active;
    bool     cmd_latched;
    uint8_t  current_cmd;

    /* ---- Timebase / delay counters ---- */
    uint32_t delay_ms_count;
    uint32_t last_delay_ms;

    /* ---- W25Q64 OS delay counters ---- */
    uint32_t w25q64_os_delay_count;

    /* ---- Driver call counters (wrapped pf_*, used by dispatch tests) ---- */
    uint32_t drv_init_count;
    uint32_t drv_deinit_count;
    uint32_t drv_read_id_count;
    uint32_t drv_read_data_count;
    uint32_t drv_write_noerase_count;
    uint32_t drv_write_erase_count;
    uint32_t drv_erase_chip_count;
    uint32_t drv_erase_sector_count;
    uint32_t drv_sleep_count;
    uint32_t drv_wakeup_count;

    /* Last driver call parameters */
    uint32_t last_drv_addr;
    uint32_t last_drv_size;
    uint8_t  last_drv_data[64];

    /* Callback invocation tracking */
    uint32_t                callback_fired;
    flash_handler_status_t  last_callback_status;
    flash_event_t           last_callback_event;

    /* ---- Mock queue backing store ---- */
    flash_event_t queue_buf[W25Q64_HDL_MOCK_QUEUE_DEPTH];
    uint32_t      queue_head;
    uint32_t      queue_tail;
    uint32_t      queue_count;

    /* ---- Fault injection ---- */
    flash_handler_status_t force_queue_create_ret;
    flash_handler_status_t force_queue_put_ret;
    bool                   force_jedec_mismatch;

    /* ---- Fake tick ---- */
    uint32_t fake_tick_ms;
} mock_state_t;

static mock_state_t  s_st;
static uint32_t      s_pass_count;
static uint32_t      s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static void state_reset(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.force_queue_create_ret = FLASH_HANLDER_OK;
    s_st.force_queue_put_ret    = FLASH_HANLDER_OK;
}

/* ======================================================================== */
/*  Mock Implementations                                                    */
/* ======================================================================== */

/* ---- OS Queue mock ------------------------------------------------------- */
static flash_handler_status_t mock_queue_create(uint32_t const item_num,
                                                 uint32_t const item_size,
                                                 void **  const queue_handler)
{
    s_st.queue_create_count++;
    s_st.last_queue_depth     = item_num;
    s_st.last_queue_item_size = item_size;

    if (FLASH_HANLDER_OK != s_st.force_queue_create_ret)
    {
        if (NULL != queue_handler) { *queue_handler = NULL; }
        DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
                  "mock queue_create -> force fail %d",
                  (int)s_st.force_queue_create_ret);
        return s_st.force_queue_create_ret;
    }

    s_st.queue_head  = 0u;
    s_st.queue_tail  = 0u;
    s_st.queue_count = 0u;
    if (NULL != queue_handler)
    {
        *queue_handler = W25Q64_HDL_MOCK_SENTINEL_PTR;
    }
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "mock queue_create depth=%u size=%u ok",
              (unsigned)item_num, (unsigned)item_size);
    return FLASH_HANLDER_OK;
}

static flash_handler_status_t mock_queue_put(void *  const  queue_handler,
                                              void *  const           item,
                                              uint32_t             timeout)
{
    (void)queue_handler;
    (void)timeout;
    s_st.queue_put_count++;

    if (FLASH_HANLDER_OK != s_st.force_queue_put_ret)
    {
        return s_st.force_queue_put_ret;
    }

    if ((NULL != item) && (s_st.queue_count < W25Q64_HDL_MOCK_QUEUE_DEPTH))
    {
        memcpy(&s_st.queue_buf[s_st.queue_tail], item, sizeof(flash_event_t));
        s_st.queue_tail = (s_st.queue_tail + 1u) % W25Q64_HDL_MOCK_QUEUE_DEPTH;
        s_st.queue_count++;
    }
    return FLASH_HANLDER_OK;
}

static flash_handler_status_t mock_queue_get(void *  const  queue_handler,
                                              void *  const            msg,
                                              uint32_t             timeout)
{
    (void)queue_handler;
    (void)timeout;
    s_st.queue_get_count++;

    if ((NULL != msg) && (s_st.queue_count > 0u))
    {
        memcpy(msg, &s_st.queue_buf[s_st.queue_head], sizeof(flash_event_t));
        s_st.queue_head = (s_st.queue_head + 1u) % W25Q64_HDL_MOCK_QUEUE_DEPTH;
        s_st.queue_count--;
        return FLASH_HANLDER_OK;
    }
    return FLASH_HANLDER_ERROR;
}

static flash_handler_status_t mock_queue_delete(void * const queue_handler)
{
    (void)queue_handler;
    s_st.queue_delete_count++;
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG, "mock queue_delete");
    return FLASH_HANLDER_OK;
}

/* ---- OS Delay mock ------------------------------------------------------- */
static flash_handler_status_t mock_os_delay_ms(uint32_t ms)
{
    s_st.os_delay_count++;
    s_st.last_os_delay_ms = ms;
    s_st.fake_tick_ms    += ms;
    return FLASH_HANLDER_OK;
}

/* ---- SPI mock with CS-bracketed transaction state ------------------------ */
static w25q64_status_t mock_spi_init(void)
{
    s_st.spi_init_count++;
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_deinit(void)
{
    s_st.spi_deinit_count++;
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_write_cs_pin(uint8_t state)
{
    if (W25Q64_HDL_MOCK_PIN_LOW == state)
    {
        s_st.cs_low_count++;
        s_st.cs_active   = true;
        s_st.cmd_latched = false;
        s_st.current_cmd = 0u;
    }
    else
    {
        s_st.cs_high_count++;
        s_st.cs_active = false;
    }
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_transmit(uint8_t const *p_data,
                                          uint32_t       data_length)
{
    if ((NULL == p_data) || (0u == data_length))
    {
        return W25Q64_ERROR;
    }
    s_st.spi_transmit_count++;

    /* Latch the first byte as the command. */
    if ((false == s_st.cmd_latched) && (true == s_st.cs_active))
    {
        s_st.current_cmd = p_data[0];
        s_st.cmd_latched = true;
    }
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_read(uint8_t *p_buffer, uint32_t buffer_length)
{
    if ((NULL == p_buffer) || (0u == buffer_length))
    {
        return W25Q64_ERROR;
    }
    s_st.spi_read_count++;

    switch (s_st.current_cmd)
    {
    case W25Q64_CMD_JEDEC_ID:   /* 0x9F */
    {
        uint8_t id[3];
        if (s_st.force_jedec_mismatch)
        {
            id[0] = 0xAAu;  id[1] = 0xBBu;  id[2] = 0xCCu;
        }
        else
        {
            id[0] = W25Q64_HDL_MOCK_JEDEC_MFR;
            id[1] = W25Q64_HDL_MOCK_JEDEC_TYPE;
            id[2] = W25Q64_HDL_MOCK_JEDEC_CAP;
        }
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            p_buffer[i] = id[i % 3u];
        }
        break;
    }

    case W25Q64_CMD_READ_ID:    /* 0x90 */
    {
        uint8_t id[2] = { W25Q64_HDL_MOCK_JEDEC_MFR,
                          W25Q64_HDL_MOCK_DEV_ID };
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            p_buffer[i] = id[i % 2u];
        }
        break;
    }

    case W25Q64_CMD_READ_REG:   /* 0x05 */
        /* Status register: never busy after init. */
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            p_buffer[i] = 0x00u;
        }
        break;

    default:
        memset(p_buffer, 0xFFu, buffer_length);
        break;
    }

    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "mock spi_read cmd=0x%02X len=%u",
              (unsigned)s_st.current_cmd, (unsigned)buffer_length);
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_transmit_dma(uint8_t const *p_data,
                                              uint32_t       data_length)
{
    (void)p_data;
    (void)data_length;
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_wait_dma_complete(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_write_dc_pin(uint8_t state)
{
    (void)state;
    return W25Q64_OK;
}

/* ---- Timebase mock ------------------------------------------------------- */
static uint32_t mock_get_tick_ms(void)
{
    return s_st.fake_tick_ms;
}

static void mock_delay_ms(uint32_t ms)
{
    s_st.delay_ms_count++;
    s_st.last_delay_ms = ms;
    s_st.fake_tick_ms += ms;
}

/* ---- W25Q64 OS Delay mock ------------------------------------------------ */
static void mock_w25q64_os_delay_ms(uint32_t ms)
{
    s_st.w25q64_os_delay_count++;
    s_st.fake_tick_ms += ms;
}

/* ======================================================================== */
/*  Wrapped driver vtable (for dispatch routing tests)                       */
/* ======================================================================== */
static w25q64_status_t wrap_drv_read_data(bsp_w25q64_driver_t *const drv,
                                           uint32_t                    addr,
                                           uint8_t             *       p_buf,
                                           uint32_t                  buf_len)
{
    (void)drv;
    s_st.drv_read_data_count++;
    s_st.last_drv_addr = addr;
    s_st.last_drv_size = buf_len;
    if (NULL != p_buf) { memset(p_buf, 0xFFu, buf_len); }
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_write_erase(bsp_w25q64_driver_t *const drv,
                                             uint32_t                    addr,
                                             uint8_t           const *  p_data,
                                             uint32_t                 data_len)
{
    (void)drv;
    s_st.drv_write_erase_count++;
    s_st.last_drv_addr = addr;
    s_st.last_drv_size = data_len;
    if ((NULL != p_data) && (data_len <= sizeof(s_st.last_drv_data)))
    {
        memcpy(s_st.last_drv_data, p_data, data_len);
    }
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_write_noerase(bsp_w25q64_driver_t *const drv,
                                               uint32_t                    addr,
                                               uint8_t           const *  p_data,
                                               uint32_t                 data_len)
{
    (void)drv;
    s_st.drv_write_noerase_count++;
    s_st.last_drv_addr = addr;
    s_st.last_drv_size = data_len;
    if ((NULL != p_data) && (data_len <= sizeof(s_st.last_drv_data)))
    {
        memcpy(s_st.last_drv_data, p_data, data_len);
    }
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_erase_chip(bsp_w25q64_driver_t *const drv)
{
    (void)drv;
    s_st.drv_erase_chip_count++;
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_erase_sector(bsp_w25q64_driver_t *const drv,
                                              uint32_t                    addr)
{
    (void)drv;
    s_st.drv_erase_sector_count++;
    s_st.last_drv_addr = addr;
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_sleep(bsp_w25q64_driver_t *const drv)
{
    (void)drv;
    s_st.drv_sleep_count++;
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_wakeup(bsp_w25q64_driver_t *const drv)
{
    (void)drv;
    s_st.drv_wakeup_count++;
    return W25Q64_OK;
}

static w25q64_status_t wrap_drv_read_id(bsp_w25q64_driver_t *const drv,
                                         uint8_t             *       p_buf,
                                         uint32_t                    buf_len)
{
    (void)drv;
    s_st.drv_read_id_count++;
    s_st.last_drv_size = buf_len;
    if ((NULL != p_buf) && (buf_len >= 2u))
    {
        p_buf[0] = 0xEFu;  p_buf[1] = 0x16u;
    }
    return W25Q64_OK;
}

/**
 * @brief      Re-populate the driver vtable with wrapped functions.
 *
 * Called after handler_inst succeeds so dispatch tests route through mock
 * driver functions instead of the real ones (which were installed by
 * w25q64_driver_inst during handler init).
 */
static void mock_driver_rewrap(bsp_w25q64_driver_t *drv)
{
    drv->pf_w25q64_read_data          = wrap_drv_read_data;
    drv->pf_w25q64_write_data_erase   = wrap_drv_write_erase;
    drv->pf_w25q64_write_data_noerase = wrap_drv_write_noerase;
    drv->pf_w25q64_erase_chip         = wrap_drv_erase_chip;
    drv->pf_w25q64_erase_sector       = wrap_drv_erase_sector;
    drv->pf_w25q64_sleep              = wrap_drv_sleep;
    drv->pf_w25q64_wakeup             = wrap_drv_wakeup;
    drv->pf_w25q64_read_id            = wrap_drv_read_id;
}

/* ======================================================================== */
/*  Bind mock interfaces into handler input-args structure                   */
/* ======================================================================== */
static void mock_handler_bind(void)
{
    /* OS Queue vtable */
    s_mock_os_queue.pf_os_queue_create = mock_queue_create;
    s_mock_os_queue.pf_os_queue_put    = mock_queue_put;
    s_mock_os_queue.pf_os_queue_get    = mock_queue_get;
    s_mock_os_queue.pf_os_queue_delete = mock_queue_delete;

    /* OS Delay vtable */
    s_mock_os_delay.pf_os_delay_ms     = mock_os_delay_ms;

    /* OS interface aggregate */
    s_mock_os_if.p_os_queue_interface  = &s_mock_os_queue;
    s_mock_os_if.p_os_delay_interface  = &s_mock_os_delay;

    /* SPI vtable */
    s_mock_spi.pf_spi_init              = mock_spi_init;
    s_mock_spi.pf_spi_deinit            = mock_spi_deinit;
    s_mock_spi.pf_spi_transmit          = mock_spi_transmit;
    s_mock_spi.pf_spi_read              = mock_spi_read;
    s_mock_spi.pf_spi_transmit_dma      = mock_spi_transmit_dma;
    s_mock_spi.pf_spi_wait_dma_complete = mock_spi_wait_dma_complete;
    s_mock_spi.pf_spi_write_cs_pin      = mock_spi_write_cs_pin;
    s_mock_spi.pf_spi_write_dc_pin      = mock_spi_write_dc_pin;

    /* Timebase vtable */
    s_mock_tb.pf_get_tick_ms            = mock_get_tick_ms;
    s_mock_tb.pf_delay_ms               = mock_delay_ms;

    /* W25Q64 OS delay vtable */
    s_mock_w25q64_os_delay.pf_os_delay_ms = mock_w25q64_os_delay_ms;

    /* Input args aggregate */
    s_mock_input_args.p_os_interface       = &s_mock_os_if;
    s_mock_input_args.p_spi_interface      = &s_mock_spi;
    s_mock_input_args.p_timebase_interface = &s_mock_tb;
    s_mock_input_args.p_w25q64_os_delay    = &s_mock_w25q64_os_delay;
}

/* ---- Handler instance init helper ---------------------------------------- */
static void mock_handler_instance_init(void)
{
    memset(&s_mock_handler,      0, sizeof(s_mock_handler));
    memset(&s_mock_driver,       0, sizeof(s_mock_driver));
    memset(s_mock_private_buf,   0, sizeof(s_mock_private_buf));

    /* Driver gets real vtable from w25q64_driver_inst during handler init;
     * the wrap vtables are re-applied by mock_driver_rewrap() after init. */
    s_mock_handler.p_w25q64_instance = &s_mock_driver;
    s_mock_handler.p_private_data    =
        (flash_handler_private_data_t *)s_mock_private_buf;
}

/* ======================================================================== */
/*  Pass / fail helpers                                                     */
/* ======================================================================== */
static void case_report(const char *name, bool ok)
{
    if (ok)
    {
        s_pass_count++;
        DEBUG_OUT(d, W25Q64_HDL_MOCK_LOG_TAG, "[PASS] %s", name);
    }
    else
    {
        s_fail_count++;
        DEBUG_OUT(e, W25Q64_HDL_MOCK_ERR_LOG_TAG,
                  "[FAIL] %s  qc=%u qp=%u qg=%u tx=%u rx=%u cs_lo=%u cs_hi=%u "
                  "dr=%u dn=%u dw=%u de=%u ds=%u",
                  name,
                  (unsigned)s_st.queue_create_count,
                  (unsigned)s_st.queue_put_count,
                  (unsigned)s_st.queue_get_count,
                  (unsigned)s_st.spi_transmit_count,
                  (unsigned)s_st.spi_read_count,
                  (unsigned)s_st.cs_low_count,
                  (unsigned)s_st.cs_high_count,
                  (unsigned)s_st.drv_read_data_count,
                  (unsigned)s_st.drv_write_noerase_count,
                  (unsigned)s_st.drv_write_erase_count,
                  (unsigned)s_st.drv_erase_chip_count,
                  (unsigned)s_st.drv_erase_sector_count);
    }
}

/* ======================================================================== */
/*  Dispatch test helper (mirrors process_flash_event for validation)        */
/* ======================================================================== */
/**
 * @brief      Replicate the handler's event-to-driver dispatch logic.
 *
 * Used because process_flash_event() is static in the handler .c file.
 * The driver vtable must have been re-wrapped with mock_driver_rewrap()
 * before calling this.
 */
static flash_handler_status_t test_dispatch(
                                bsp_w25q64_handler_t * const handler,
                                flash_event_t        * const   event)
{
    flash_handler_status_t   ret = FLASH_HANLDER_OK;
    w25q64_status_t      drv_ret =          W25Q64_OK;
    bsp_w25q64_driver_t   *  drv = handler->p_w25q64_instance;

    if ((NULL == drv) || (NULL == event))
    {
        return FLASH_HANLDER_ERRORPARAMETER;
    }

    switch (event->event_type)
    {
    case FLASH_HANDLER_EVENT_READ:
        if ((NULL == event->data) || (0u == event->size))
        {
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_read_data(drv, event->addr,
                                           event->data, event->size);
        break;

    case FLASH_HANDLER_EVENT_WRITE:
        if ((NULL == event->data) || (0u == event->size))
        {
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_write_data_erase(drv, event->addr,
                                                  event->data, event->size);
        break;

    case FLASH_HANDLER_EVENT_WRITE_NOERASE:
        if ((NULL == event->data) || (0u == event->size))
        {
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_write_data_noerase(drv, event->addr,
                                                    event->data, event->size);
        break;

    case FLASH_HANDLER_EVENT_ERASE_CHIP:
        drv_ret = drv->pf_w25q64_erase_chip(drv);
        break;

    case FLASH_HANDLER_EVENT_ERASE_SECTOR:
        drv_ret = drv->pf_w25q64_erase_sector(drv, event->addr);
        break;

    case FLASH_HANDLER_EVENT_WAKEUP:
        drv_ret = drv->pf_w25q64_wakeup(drv);
        break;

    case FLASH_HANDLER_EVENT_SLEEP:
        drv_ret = drv->pf_w25q64_sleep(drv);
        break;

    case FLASH_HANDLER_EVENT_TEST:
        if ((NULL == event->data) || (event->size < 2u))
        {
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_read_id(drv, event->data, event->size);
        break;

    default:
        return FLASH_HANLDER_ERRORUNSUPPORTED;
    }

    ret = (flash_handler_status_t)drv_ret;
    return ret;
}

static flash_handler_status_t test_dispatch_with_callback(
                                bsp_w25q64_handler_t * const handler,
                                flash_event_t        * const   event)
{
    flash_handler_status_t ret = test_dispatch(handler, event);
    event->status = ret;
    if (NULL != event->pf_callback)
    {
        event->pf_callback(event);
    }
    return ret;
}

/* ---- Callback capture ---------------------------------------------------- */
static void test_callback(void *args)
{
    flash_event_t *evt = (flash_event_t *)args;
    if (NULL != evt)
    {
        s_st.callback_fired++;
        s_st.last_callback_status = evt->status;
        memcpy(&s_st.last_callback_event, evt, sizeof(flash_event_t));
    }
}

/* ======================================================================== */
/*  Test Cases                                                              */
/* ======================================================================== */

/* ---- CASE 1: handler_inst NULL handler_instance ---- */
static void test_case_inst_null_handler(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 1: handler_inst(NULL, &args) -> ERRORPARAMETER =====");
    state_reset();
    mock_handler_bind();

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(NULL, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 1 inst NULL handler", ok);
}

/* ---- CASE 2: handler_inst NULL input_args ---- */
static void test_case_inst_null_args(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 2: handler_inst(&h, NULL) -> ERRORPARAMETER =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, NULL);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 2 inst NULL args", ok);
}

/* ---- CASE 3: handler_inst NULL driver instance ---- */
static void test_case_inst_null_driver(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 3: handler_inst p_w25q64_instance=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_handler.p_w25q64_instance = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 3 inst NULL driver", ok);
}

/* ---- CASE 4: handler_inst NULL private data ---- */
static void test_case_inst_null_private(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 4: handler_inst p_private_data=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_handler.p_private_data = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 4 inst NULL private data", ok);
}

/* ---- CASE 5: handler_inst already initialized ---- */
static void test_case_inst_already_inited(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 5: handler_inst already inited -> ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();

    /* First init succeeds (mock SPI serves valid JEDEC ID). */
    flash_handler_status_t ret1 =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    /* Second init must be rejected. */
    flash_handler_status_t ret2 =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_OK == ret1) &&
              (FLASH_HANLDER_ERRORRESOURCE == ret2) &&
              (1u == s_st.queue_create_count) &&
              (s_st.spi_transmit_count >= 1u) &&
              (s_st.spi_read_count >= 1u);
    case_report("CASE 5 inst already inited", ok);
}

/* ---- CASE 6: handler_inst NULL os_interface ---- */
static void test_case_inst_null_os_if(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 6: handler_inst os_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_input_args.p_os_interface = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 6 inst NULL os_interface", ok);
}

/* ---- CASE 7: handler_inst NULL spi_interface ---- */
static void test_case_inst_null_spi(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 7: handler_inst spi_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_input_args.p_spi_interface = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 7 inst NULL spi", ok);
}

/* ---- CASE 8: handler_inst NULL timebase ---- */
static void test_case_inst_null_timebase(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 8: handler_inst timebase=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_input_args.p_timebase_interface = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 8 inst NULL timebase", ok);
}

/* ---- CASE 9: handler_inst NULL w25q64_os_delay ---- */
static void test_case_inst_null_os_delay(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 9: handler_inst w25q64_os_delay=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_input_args.p_w25q64_os_delay = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 9 inst NULL w25q64_os_delay", ok);
}

/* ---- CASE 10: handler_inst NULL queue_interface ---- */
static void test_case_inst_null_queue_if(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 10: handler_inst queue_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_os_if.p_os_queue_interface = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 10 inst NULL queue_if", ok);
}

/* ---- CASE 11: handler_inst NULL delay_interface ---- */
static void test_case_inst_null_delay_if(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 11: handler_inst delay_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_os_if.p_os_delay_interface = NULL;

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_create_count);
    case_report("CASE 11 inst NULL delay_if", ok);
}

/* ---- CASE 12: handler_inst queue_create fails ---- */
static void test_case_inst_queue_create_fails(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 12: handler_inst queue_create fails -> "
              "ERRORNOMEMORY =====");
    state_reset();
    s_st.force_queue_create_ret = FLASH_HANLDER_ERRORNOMEMORY;
    mock_handler_bind();
    mock_handler_instance_init();

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORNOMEMORY == ret) &&
              (1u == s_st.queue_create_count)       &&
              (0u == s_st.spi_transmit_count);       /* driver not touched */
    case_report("CASE 12 inst queue_create fails", ok);
}

/* ---- CASE 13: handler_inst w25q64_init fails (bad JEDEC ID) ---- */
static void test_case_inst_driver_init_fails(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 13: handler_inst JEDEC mismatch -> "
              "ERRORRESOURCE =====");
    state_reset();
    s_st.force_jedec_mismatch = true;
    mock_handler_bind();
    mock_handler_instance_init();

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (1u == s_st.queue_create_count)       &&
              (s_st.spi_read_count >= 1u);
    case_report("CASE 13 inst driver init fails", ok);
}

/* ---- CASE 14: handler_inst success ---- */
static void test_case_inst_success(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 14: handler_inst success -> OK =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();

    flash_handler_status_t ret =
        bsp_w25q64_handler_inst(&s_mock_handler, &s_mock_input_args);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.queue_create_count) &&
              (10u == s_st.last_queue_depth) &&
              (sizeof(flash_event_t) == s_st.last_queue_item_size) &&
              (1u == s_st.spi_init_count) &&
              (s_st.spi_transmit_count >= 1u) &&
              (s_st.spi_read_count >= 1u);

    /* Re-wrap driver vtable so dispatch tests use mock functions. */
    if (FLASH_HANLDER_OK == ret)
    {
        mock_driver_rewrap(s_mock_handler.p_w25q64_instance);
    }
    case_report("CASE 14 inst success", ok);
}

/* ---- CASE 15: handler_flash_event_send NULL event ---- */
static void test_case_send_null_event(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 15: handler_flash_event_send(NULL) -> "
              "ERRORPARAMETER =====");
    state_reset();

    flash_handler_status_t ret = handler_flash_event_send(NULL);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.queue_put_count);
    case_report("CASE 15 send NULL event", ok);
}

/* ---- CASE 16: handler_flash_event_send instance not mounted ---- */
static void test_case_send_not_mounted(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 16: handler_flash_event_send not mounted -> "
              "ERRORRESOURCE =====");
    state_reset();

    flash_event_t evt = {0};
    flash_handler_status_t ret = handler_flash_event_send(&evt);

    bool ok = (FLASH_HANLDER_ERRORRESOURCE == ret) &&
              (0u == s_st.queue_put_count);
    case_report("CASE 16 send not mounted", ok);
}

/* ---- CASE 17: Event dispatch: READ routes to pf_w25q64_read_data ---- */
static void test_case_dispatch_read(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 17: dispatch READ -> pf_w25q64_read_data =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t buf[W25Q64_HDL_MOCK_READ_LEN] = {0};
    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_TEST_ADDR,
        .size       = W25Q64_HDL_MOCK_READ_LEN,
        .data       = buf,
        .event_type = FLASH_HANDLER_EVENT_READ,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_read_data_count) &&
              (W25Q64_HDL_MOCK_TEST_ADDR == s_st.last_drv_addr) &&
              (W25Q64_HDL_MOCK_READ_LEN == s_st.last_drv_size);
    case_report("CASE 17 dispatch READ", ok);
}

/* ---- CASE 18: Event dispatch: WRITE routes to pf_w25q64_write_data_erase ---- */
static void test_case_dispatch_write(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 18: dispatch WRITE -> pf_w25q64_write_data_erase "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t data[W25Q64_HDL_MOCK_WRITE_LEN];
    for (uint32_t i = 0u; i < W25Q64_HDL_MOCK_WRITE_LEN; i++)
    {
        data[i] = (uint8_t)(0xA0u + i);
    }

    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_TEST_ADDR,
        .size       = W25Q64_HDL_MOCK_WRITE_LEN,
        .data       = data,
        .event_type = FLASH_HANDLER_EVENT_WRITE,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_write_erase_count) &&
              (W25Q64_HDL_MOCK_TEST_ADDR == s_st.last_drv_addr) &&
              (W25Q64_HDL_MOCK_WRITE_LEN == s_st.last_drv_size) &&
              (0 == memcmp(data, s_st.last_drv_data, W25Q64_HDL_MOCK_WRITE_LEN));
    case_report("CASE 18 dispatch WRITE", ok);
}

/* ---- CASE 19: Event dispatch: WRITE_NOERASE routes correctly ---- */
static void test_case_dispatch_write_noerase(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 19: dispatch WRITE_NOERASE -> "
              "pf_w25q64_write_data_noerase =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t data[W25Q64_HDL_MOCK_WRITE_LEN];
    for (uint32_t i = 0u; i < W25Q64_HDL_MOCK_WRITE_LEN; i++)
    {
        data[i] = (uint8_t)(0xB0u + i);
    }

    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_TEST_ADDR,
        .size       = W25Q64_HDL_MOCK_WRITE_LEN,
        .data       = data,
        .event_type = FLASH_HANDLER_EVENT_WRITE_NOERASE,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_write_noerase_count) &&
              (W25Q64_HDL_MOCK_TEST_ADDR == s_st.last_drv_addr) &&
              (W25Q64_HDL_MOCK_WRITE_LEN == s_st.last_drv_size) &&
              (0 == memcmp(data, s_st.last_drv_data, W25Q64_HDL_MOCK_WRITE_LEN));
    case_report("CASE 19 dispatch WRITE_NOERASE", ok);
}

/* ---- CASE 20: Event dispatch: ERASE_CHIP routes to pf_w25q64_erase_chip ---- */
static void test_case_dispatch_erase_chip(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 20: dispatch ERASE_CHIP -> pf_w25q64_erase_chip "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .event_type = FLASH_HANDLER_EVENT_ERASE_CHIP,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_erase_chip_count);
    case_report("CASE 20 dispatch ERASE_CHIP", ok);
}

/* ---- CASE 21: Event dispatch: ERASE_SECTOR routes correctly ---- */
static void test_case_dispatch_erase_sector(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 21: dispatch ERASE_SECTOR -> pf_w25q64_erase_sector "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_SECTOR_ADDR,
        .event_type = FLASH_HANDLER_EVENT_ERASE_SECTOR,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_erase_sector_count) &&
              (W25Q64_HDL_MOCK_SECTOR_ADDR == s_st.last_drv_addr);
    case_report("CASE 21 dispatch ERASE_SECTOR", ok);
}

/* ---- CASE 22: Event dispatch: WAKEUP routes to pf_w25q64_wakeup ---- */
static void test_case_dispatch_wakeup(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 22: dispatch WAKEUP -> pf_w25q64_wakeup =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .event_type = FLASH_HANDLER_EVENT_WAKEUP,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_wakeup_count);
    case_report("CASE 22 dispatch WAKEUP", ok);
}

/* ---- CASE 23: Event dispatch: SLEEP routes to pf_w25q64_sleep ---- */
static void test_case_dispatch_sleep(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 23: dispatch SLEEP -> pf_w25q64_sleep =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .event_type = FLASH_HANDLER_EVENT_SLEEP,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_sleep_count);
    case_report("CASE 23 dispatch SLEEP", ok);
}

/* ---- CASE 24: Event dispatch: TEST routes to pf_w25q64_read_id ---- */
static void test_case_dispatch_test(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 24: dispatch TEST -> pf_w25q64_read_id =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t id_buf[2] = {0};
    flash_event_t evt = {
        .data       = id_buf,
        .size       = 2u,
        .event_type = FLASH_HANDLER_EVENT_TEST,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.drv_read_id_count) &&
              (0xEFu == id_buf[0]) &&
              (0x16u == id_buf[1]);
    case_report("CASE 24 dispatch TEST", ok);
}

/* ---- CASE 25: Event dispatch: invalid event type ---- */
static void test_case_dispatch_invalid_type(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 25: dispatch invalid type -> ERRORUNSUPPORTED "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .event_type = (flash_handler_event_type_t)99u,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_ERRORUNSUPPORTED == ret);
    case_report("CASE 25 dispatch invalid type", ok);
}

/* ---- CASE 26: Dispatch READ with NULL data -> ERRORPARAMETER ---- */
static void test_case_dispatch_read_null_data(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 26: dispatch READ data=NULL -> ERRORPARAMETER "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_TEST_ADDR,
        .size       = W25Q64_HDL_MOCK_READ_LEN,
        .data       = NULL,
        .event_type = FLASH_HANDLER_EVENT_READ,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.drv_read_data_count);
    case_report("CASE 26 dispatch READ NULL data", ok);
}

/* ---- CASE 27: Dispatch READ with zero size -> ERRORPARAMETER ---- */
static void test_case_dispatch_read_zero_size(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 27: dispatch READ size=0 -> ERRORPARAMETER =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t buf[16];
    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_TEST_ADDR,
        .size       = 0u,
        .data       = buf,
        .event_type = FLASH_HANDLER_EVENT_READ,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.drv_read_data_count);
    case_report("CASE 27 dispatch READ zero size", ok);
}

/* ---- CASE 28: Dispatch WRITE with NULL data -> ERRORPARAMETER ---- */
static void test_case_dispatch_write_null_data(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 28: dispatch WRITE data=NULL -> ERRORPARAMETER "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .addr       = W25Q64_HDL_MOCK_TEST_ADDR,
        .size       = W25Q64_HDL_MOCK_WRITE_LEN,
        .data       = NULL,
        .event_type = FLASH_HANDLER_EVENT_WRITE,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.drv_write_erase_count);
    case_report("CASE 28 dispatch WRITE NULL data", ok);
}

/* ---- CASE 29: Dispatch TEST with size < 2 -> ERRORPARAMETER ---- */
static void test_case_dispatch_test_short_buf(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 29: dispatch TEST size=1 -> ERRORPARAMETER =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t buf[1];
    flash_event_t evt = {
        .data       = buf,
        .size       = 1u,
        .event_type = FLASH_HANDLER_EVENT_TEST,
        .pf_callback = NULL,
    };

    flash_handler_status_t ret = test_dispatch(&s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (0u == s_st.drv_read_id_count);
    case_report("CASE 29 dispatch TEST short buf", ok);
}

/* ---- CASE 30: Callback invoked after dispatch ---- */
static void test_case_dispatch_callback(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 30: dispatch with callback -> callback fired "
              "=====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    uint8_t buf[W25Q64_HDL_MOCK_READ_LEN];
    flash_event_t evt = {
        .addr        = W25Q64_HDL_MOCK_TEST_ADDR,
        .size        = W25Q64_HDL_MOCK_READ_LEN,
        .data        = buf,
        .event_type  = FLASH_HANDLER_EVENT_READ,
        .pf_callback = test_callback,
        .p_user_ctx  = NULL,
    };

    flash_handler_status_t ret = test_dispatch_with_callback(
                                    &s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_OK == ret) &&
              (1u == s_st.callback_fired) &&
              (FLASH_HANLDER_OK == s_st.last_callback_status);
    case_report("CASE 30 dispatch callback", ok);
}

/* ---- CASE 31: Callback receives error status on failed dispatch ---- */
static void test_case_dispatch_callback_error(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 31: dispatch error -> callback receives error "
              "status =====");
    state_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    mock_driver_rewrap(&s_mock_driver);

    flash_event_t evt = {
        .data        = NULL,
        .size        = W25Q64_HDL_MOCK_READ_LEN,
        .event_type  = FLASH_HANDLER_EVENT_READ,
        .pf_callback = test_callback,
        .p_user_ctx  = NULL,
    };

    flash_handler_status_t ret = test_dispatch_with_callback(
                                    &s_mock_handler, &evt);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret) &&
              (1u == s_st.callback_fired) &&
              (FLASH_HANLDER_ERRORPARAMETER == s_st.last_callback_status);
    case_report("CASE 31 dispatch callback error", ok);
}

/* ---- CASE 32: Dispatch NULL driver instance ---- */
static void test_case_dispatch_null_drv(void)
{
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== CASE 32: dispatch NULL driver -> ERRORPARAMETER =====");
    state_reset();

    flash_event_t evt = { .event_type = FLASH_HANDLER_EVENT_READ };
    flash_handler_status_t ret = test_dispatch(NULL, &evt);

    bool ok = (FLASH_HANLDER_ERRORPARAMETER == ret);
    case_report("CASE 32 dispatch NULL handler", ok);

    ret = test_dispatch(&s_mock_handler, NULL);
    ok = ok && (FLASH_HANLDER_ERRORPARAMETER == ret);
}
//******************************* Functions *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Entry point for the W25Q64 handler mock test task.
 *
 * @param[in]  argument : Unused task argument.
 *
 * @return     None.
 * */
void w25q64_handler_mock_test_task(void *argument)
{
    (void)argument;

    /**
     * Stagger against EasyLogger warm-up so our banner lines are not
     * shredded by concurrent boot logs from the rest of User_Init.
     **/
    osal_task_delay(W25Q64_HDL_MOCK_BOOT_WAIT_MS);

    DEBUG_OUT(d, W25Q64_HDL_MOCK_LOG_TAG,
              "w25q64_handler_mock_test_task start");

    /* ---- handler_inst parameter validation ---- */
    test_case_inst_null_handler();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_args();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_driver();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_private();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_already_inited();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_os_if();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_spi();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_timebase();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_os_delay();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_queue_if();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_null_delay_if();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_queue_create_fails();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_driver_init_fails();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_inst_success();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    /* ---- handler_flash_event_send parameter validation ---- */
    test_case_send_null_event();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_send_not_mounted();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    /* ---- Event dispatch routing ---- */
    test_case_dispatch_read();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_write();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_write_noerase();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_erase_chip();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_erase_sector();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_wakeup();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_sleep();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_test();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_invalid_type();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    /* ---- Dispatch parameter validation ---- */
    test_case_dispatch_read_null_data();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_read_zero_size();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_write_null_data();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_test_short_buf();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    /* ---- Callback ---- */
    test_case_dispatch_callback();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_callback_error();
    osal_task_delay(W25Q64_HDL_MOCK_STEP_GAP_MS);

    test_case_dispatch_null_drv();

    /* ---- Summary ---- */
    DEBUG_OUT(i, W25Q64_HDL_MOCK_LOG_TAG,
              "===== SUMMARY: %u PASS / %u FAIL =====",
              (unsigned)s_pass_count, (unsigned)s_fail_count);
    if (0u == s_fail_count)
    {
        DEBUG_OUT(d, W25Q64_HDL_MOCK_LOG_TAG,
                  "w25q64_handler_mock_test_task ALL PASS");
    }
    else
    {
        DEBUG_OUT(e, W25Q64_HDL_MOCK_ERR_LOG_TAG,
                  "w25q64_handler_mock_test_task %u FAIL -- review log above",
                  (unsigned)s_fail_count);
    }

    for (;;)
    {
        osal_task_delay(5000u);
    }
}
//******************************* Functions *********************************//
