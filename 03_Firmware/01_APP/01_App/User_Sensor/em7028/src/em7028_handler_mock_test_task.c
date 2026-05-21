/******************************************************************************
 * @file em7028_handler_mock_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_em7028_handler.h
 * - bsp_em7028_driver.h
 * - bsp_em7028_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 handler logic validation without real hardware.
 *
 * Processing flow:
 * Mount fake OS-queue / IIC / timebase / delay / OS-delay vtables.
 * The IIC mock serves the EM7028 chip-id 0x36 so the real
 * bsp_em7028_driver_inst (auto-invoked by handler internal_init) can
 * succeed.  After a successful inst the driver vtable is re-wrapped
 * with counting stubs so dispatch routing tests stay in pure-logic
 * land. process_event and the sample block are static inside the
 * handler .c, so the test mirrors them with test_process_event /
 * test_sample_tick helpers (kept byte-for-byte aligned with the
 * handler implementation -- update this file alongside any change to
 * bsp_em7028_handler.c).
 *
 * @version V1.0 2026-04-29
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
#include "bsp_em7028_handler.h"
#include "bsp_em7028_driver.h"
#include "bsp_em7028_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define EM7028_HDL_MOCK_BOOT_WAIT_MS                2500U
#define EM7028_HDL_MOCK_STEP_GAP_MS                  150U

#define EM7028_HDL_MOCK_CMD_DEPTH                      4U  /* matches .c     */
#define EM7028_HDL_MOCK_FRAME_DEPTH                   10U  /* matches .c     */

#define EM7028_HDL_MOCK_PRIV_BUF_SIZE                 64U
#define EM7028_HDL_MOCK_QUEUE_BUF_SIZE               512U  /* per queue      */
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/* ---- Mock queue backing ----------------------------------------------- */
typedef struct
{
    bool     in_use;
    uint32_t item_size;
    uint32_t depth;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint8_t  buf[EM7028_HDL_MOCK_QUEUE_BUF_SIZE];
} mock_queue_t;

#define EM7028_HDL_MOCK_MAX_QUEUES                    2U
static mock_queue_t s_mock_queues[EM7028_HDL_MOCK_MAX_QUEUES];

/* ---- Per-case capture ------------------------------------------------- */
typedef struct
{
    /* OS-queue counters */
    uint32_t queue_create_count;
    uint32_t queue_put_count;
    uint32_t queue_get_count;
    uint32_t queue_delete_count;
    uint32_t last_create_depth;
    uint32_t last_create_item_size;
    void    *cmd_queue_handle;
    void    *frame_queue_handle;

    /* IIC counters (used during driver auto-init) */
    uint32_t iic_init_count;
    uint32_t iic_deinit_count;
    uint32_t iic_write_count;
    uint32_t iic_read_count;

    /* Timebase / delay counters */
    uint32_t fake_tick_ms;
    uint32_t delay_ms_count;
    uint32_t delay_us_count;
    uint32_t os_delay_count;

    /* Wrapped driver counters (re-applied after inst) */
    uint32_t drv_init_count;
    uint32_t drv_deinit_count;
    uint32_t drv_start_count;
    uint32_t drv_stop_count;
    uint32_t drv_soft_reset_count;
    uint32_t drv_read_id_count;
    uint32_t drv_apply_config_count;
    uint32_t drv_read_frame_count;

    /* Last apply_config arg captured */
    em7028_config_t last_apply_cfg;

    /* Fault injection */
    em7028_handler_status_t force_create_ret;
    uint32_t                create_fail_on_call;   /* 0 = never; N = Nth   */
    em7028_status_t         force_read_frame_ret;
} mock_state_t;

static mock_state_t s_st;

/* ---- Mock interface instances ----------------------------------------- */
static bsp_em7028_handler_t          s_mock_handler;
static bsp_em7028_driver_t           s_mock_driver;
static uint8_t  s_mock_priv_buf[EM7028_HDL_MOCK_PRIV_BUF_SIZE];

static em7028_handler_input_args_t   s_mock_args;
static em7028_handler_os_interface_t s_mock_os_if;
static em7028_handler_os_queue_t     s_mock_os_queue;
static em7028_iic_driver_interface_t s_mock_iic;
static em7028_timebase_interface_t          s_mock_tb;
static em7028_delay_interface_t      s_mock_delay;
static em7028_os_delay_interface_t   s_mock_os_delay;

/* Local-mirror state for dispatch tests (the handler's own is_active is
 * inside the opaque private struct and not visible from this TU).        */
static bool s_test_is_active = false;

static uint32_t s_pass_count;
static uint32_t s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/* ---- Reset helpers ----------------------------------------------------- */
/**
 * @brief      Clear all per-case capture state.
 *
 * @return     None.
 * */
static void state_reset(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.force_create_ret     = EM7028_HANDLER_OK;
    s_st.force_read_frame_ret = EM7028_OK;
}

/**
 * @brief      Clear the queue backing pool so the next inst can
 *             re-allocate cmd + frame queues from a fresh pool.
 *
 * @return     None.
 * */
static void mock_queues_reset(void)
{
    memset(s_mock_queues, 0, sizeof(s_mock_queues));
}

/* ======================================================================== */
/*  OS Queue mock                                                           */
/* ======================================================================== */
/**
 * @brief      Allocate a backing slot from the queue pool. Returns the
 *             slot pointer as the "handle" so put / get can route back.
 *             Honours create_fail_on_call fault injection.
 * */
static em7028_handler_status_t mock_queue_create(uint32_t const item_num,
                                                  uint32_t const item_size,
                                                  void   ** const  p_handle)
{
    s_st.queue_create_count++;
    s_st.last_create_depth     = item_num;
    s_st.last_create_item_size = item_size;

    if ((0U != s_st.create_fail_on_call) &&
        (s_st.queue_create_count == s_st.create_fail_on_call))
    {
        if (NULL != p_handle)
        {
            *p_handle = NULL;
        }
        return s_st.force_create_ret;
    }

    /* Find a free slot in the pool. */
    mock_queue_t *q = NULL;
    for (uint32_t i = 0U; i < EM7028_HDL_MOCK_MAX_QUEUES; i++)
    {
        if (!s_mock_queues[i].in_use)
        {
            q = &s_mock_queues[i];
            break;
        }
    }
    if (NULL == q)
    {
        return EM7028_HANDLER_ERRORNOMEMORY;
    }

    if ((item_num * item_size) > EM7028_HDL_MOCK_QUEUE_BUF_SIZE)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "mock queue_create depth*size > backing buf");
        return EM7028_HANDLER_ERRORNOMEMORY;
    }

    q->in_use    = true;
    q->item_size = item_size;
    q->depth     = item_num;
    q->head      = 0U;
    q->tail      = 0U;
    q->count     = 0U;

    if (NULL != p_handle)
    {
        *p_handle = (void *)q;
    }

    /* First create -> cmd queue, second -> frame queue.                  */
    if (NULL == s_st.cmd_queue_handle)
    {
        s_st.cmd_queue_handle = (void *)q;
    }
    else if (NULL == s_st.frame_queue_handle)
    {
        s_st.frame_queue_handle = (void *)q;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "mock queue_create depth=%u size=%u ok",
              (unsigned)item_num, (unsigned)item_size);
    return EM7028_HANDLER_OK;
}

/**
 * @brief      Push an item into the named mock queue (drop on full).
 * */
static em7028_handler_status_t mock_queue_put(void   * const queue_handler,
                                               void  * const          item,
                                               uint32_t            timeout)
{
    (void)timeout;
    s_st.queue_put_count++;

    mock_queue_t *q = (mock_queue_t *)queue_handler;
    if ((NULL == q) || (NULL == item))
    {
        return EM7028_HANDLER_ERRORPARAMETER;
    }
    if (q->count >= q->depth)
    {
        return EM7028_HANDLER_ERROR;
    }

    memcpy(&q->buf[q->tail * q->item_size], item, q->item_size);
    q->tail = (q->tail + 1U) % q->depth;
    q->count++;
    return EM7028_HANDLER_OK;
}

/**
 * @brief      Pop an item from the named mock queue (no real timeout).
 * */
static em7028_handler_status_t mock_queue_get(void   * const queue_handler,
                                               void  * const           msg,
                                               uint32_t            timeout)
{
    (void)timeout;
    s_st.queue_get_count++;

    mock_queue_t *q = (mock_queue_t *)queue_handler;
    if ((NULL == q) || (NULL == msg))
    {
        return EM7028_HANDLER_ERRORPARAMETER;
    }
    if (0U == q->count)
    {
        return EM7028_HANDLER_ERRORTIMEOUT;
    }

    memcpy(msg, &q->buf[q->head * q->item_size], q->item_size);
    q->head = (q->head + 1U) % q->depth;
    q->count--;
    return EM7028_HANDLER_OK;
}

static em7028_handler_status_t mock_queue_delete(void * const queue_handler)
{
    (void)queue_handler;
    s_st.queue_delete_count++;
    return EM7028_HANDLER_OK;
}

/* ======================================================================== */
/*  IIC mock -- enough to let bsp_em7028_driver_inst auto-init succeed      */
/* ======================================================================== */
static em7028_status_t mock_iic_init(void *hi2c)
{
    (void)hi2c;
    s_st.iic_init_count++;
    return EM7028_OK;
}

static em7028_status_t mock_iic_deinit(void *hi2c)
{
    (void)hi2c;
    s_st.iic_deinit_count++;
    return EM7028_OK;
}

static em7028_status_t mock_iic_mem_write(void    *i2c,
                                          uint16_t des_addr,
                                          uint16_t mem_addr,
                                          uint16_t mem_size,
                                          uint8_t *p_data,
                                          uint16_t size,
                                          uint32_t timeout)
{
    (void)i2c; (void)des_addr; (void)mem_addr; (void)mem_size;
    (void)p_data; (void)size; (void)timeout;
    s_st.iic_write_count++;
    return EM7028_OK;
}

static em7028_status_t mock_iic_mem_read(void    *i2c,
                                         uint16_t des_addr,
                                         uint16_t mem_addr,
                                         uint16_t mem_size,
                                         uint8_t *p_data,
                                         uint16_t size,
                                         uint32_t timeout)
{
    (void)i2c; (void)des_addr; (void)mem_size; (void)timeout;
    s_st.iic_read_count++;

    /**
     * Driver auto-init reads ID_REG (0x00) and expects 0x36. Serve that
     * for any read of register 0; zero-fill everything else so HRS data
     * register reads (if any) do not return junk.
     **/
    if ((NULL != p_data) && (size > 0U))
    {
        memset(p_data, 0, size);
        if (ID_REG == mem_addr)
        {
            p_data[0] = EM7028_ID;
        }
    }
    return EM7028_OK;
}

/* ======================================================================== */
/*  Timebase / delay / OS-delay mocks                                       */
/* ======================================================================== */
static uint32_t mock_get_tick_count(void)
{
    return s_st.fake_tick_ms;
}

static void mock_delay_init(void) { /* no-op */ }

static void mock_delay_ms(uint32_t const ms)
{
    s_st.delay_ms_count++;
    s_st.fake_tick_ms += ms;
}

static void mock_delay_us(uint32_t const us)
{
    (void)us;
    s_st.delay_us_count++;
}

static void mock_rtos_delay(uint32_t const ms)
{
    s_st.os_delay_count++;
    s_st.fake_tick_ms += ms;
}

/* ======================================================================== */
/*  Wrapped driver vtable (for dispatch routing tests, applied AFTER inst)  */
/* ======================================================================== */
static em7028_status_t wrap_drv_init(struct bsp_em7028_driver *const self)
{
    (void)self;
    s_st.drv_init_count++;
    return EM7028_OK;
}
static em7028_status_t wrap_drv_deinit(struct bsp_em7028_driver *const self)
{
    (void)self;
    s_st.drv_deinit_count++;
    return EM7028_OK;
}
static em7028_status_t wrap_drv_soft_reset(struct bsp_em7028_driver *const self)
{
    (void)self;
    s_st.drv_soft_reset_count++;
    return EM7028_OK;
}
static em7028_status_t wrap_drv_read_id(struct bsp_em7028_driver *const self,
                                         uint8_t                  *const id)
{
    (void)self;
    s_st.drv_read_id_count++;
    if (NULL != id) { *id = EM7028_ID; }
    return EM7028_OK;
}
static em7028_status_t wrap_drv_apply_config(
    struct bsp_em7028_driver *const self,
    em7028_config_t    const *const p_cfg)
{
    (void)self;
    s_st.drv_apply_config_count++;
    if (NULL != p_cfg)
    {
        s_st.last_apply_cfg = *p_cfg;
    }
    return EM7028_OK;
}
static em7028_status_t wrap_drv_start(struct bsp_em7028_driver *const self)
{
    (void)self;
    s_st.drv_start_count++;
    return EM7028_OK;
}
static em7028_status_t wrap_drv_stop(struct bsp_em7028_driver *const self)
{
    (void)self;
    s_st.drv_stop_count++;
    return EM7028_OK;
}
static em7028_status_t wrap_drv_read_frame(
    struct bsp_em7028_driver *const self,
    em7028_ppg_frame_t       *const frame)
{
    (void)self;
    s_st.drv_read_frame_count++;

    if (s_st.force_read_frame_ret != EM7028_OK)
    {
        return s_st.force_read_frame_ret;
    }
    if (NULL != frame)
    {
        memset(frame, 0, sizeof(*frame));
        frame->timestamp_ms = s_st.fake_tick_ms;
        frame->hrs1_pixel[0] = (uint16_t)s_st.drv_read_frame_count;
        frame->hrs1_sum      = (uint32_t)s_st.drv_read_frame_count;
    }
    return EM7028_OK;
}

/**
 * @brief      Re-populate the driver vtable with counting wrappers.
 *             Called after handler_inst succeeds so dispatch tests do
 *             not pour through the real driver / I2C path again.
 * */
static void mock_driver_rewrap(bsp_em7028_driver_t *drv)
{
    drv->pf_init         = wrap_drv_init;
    drv->pf_deinit       = wrap_drv_deinit;
    drv->pf_soft_reset   = wrap_drv_soft_reset;
    drv->pf_read_id      = wrap_drv_read_id;
    drv->pf_apply_config = wrap_drv_apply_config;
    drv->pf_start        = wrap_drv_start;
    drv->pf_stop         = wrap_drv_stop;
    drv->pf_read_frame   = wrap_drv_read_frame;
}

/* ======================================================================== */
/*  Bind mock interfaces into handler input-args structure                  */
/* ======================================================================== */
/**
 * @brief      Wire all mock vtables into the input_args / handler_instance
 *             so a fresh inst can be attempted from the next test case.
 * */
static void mock_handler_bind(void)
{
    /* OS queue */
    s_mock_os_queue.pf_os_queue_create = mock_queue_create;
    s_mock_os_queue.pf_os_queue_put    = mock_queue_put;
    s_mock_os_queue.pf_os_queue_get    = mock_queue_get;
    s_mock_os_queue.pf_os_queue_delete = mock_queue_delete;

    s_mock_os_if.p_os_queue_interface  = &s_mock_os_queue;

    /* IIC */
    s_mock_iic.hi2c              = (void *)0xE7028012u;
    s_mock_iic.pf_iic_init       = mock_iic_init;
    s_mock_iic.pf_iic_deinit     = mock_iic_deinit;
    s_mock_iic.pf_iic_mem_write  = mock_iic_mem_write;
    s_mock_iic.pf_iic_mem_read   = mock_iic_mem_read;

    /* Timebase */
    s_mock_tb.pf_get_tick_count  = mock_get_tick_count;

    /* DWT delay */
    s_mock_delay.pf_delay_init   = mock_delay_init;
    s_mock_delay.pf_delay_ms     = mock_delay_ms;
    s_mock_delay.pf_delay_us     = mock_delay_us;

    /* OS delay */
    s_mock_os_delay.pf_rtos_delay = mock_rtos_delay;

    /* Aggregate */
    s_mock_args.p_os_interface       = &s_mock_os_if;
    s_mock_args.p_iic_interface      = &s_mock_iic;
    s_mock_args.p_timebase_interface = &s_mock_tb;
    s_mock_args.p_delay_interface    = &s_mock_delay;
    s_mock_args.p_driver_os_delay    = &s_mock_os_delay;
}

/**
 * @brief      Zero the handler / driver / private storage and mount
 *             the driver + private slots onto the handler instance.
 *             Does NOT bind interfaces or run inst.
 * */
static void mock_handler_instance_init(void)
{
    memset(&s_mock_handler, 0, sizeof(s_mock_handler));
    memset(&s_mock_driver,  0, sizeof(s_mock_driver));
    memset(s_mock_priv_buf, 0, sizeof(s_mock_priv_buf));

    s_mock_handler.p_em7028_instance = &s_mock_driver;
    s_mock_handler.p_private_data    =
        (em7028_handler_private_t *)s_mock_priv_buf;
}

/* ======================================================================== */
/*  Dispatch / sample-tick mirrors (kept aligned with handler.c)            */
/* ======================================================================== */
/**
 * @brief      Mirror of process_event() in bsp_em7028_handler.c.
 *             Mutates s_test_is_active so RECONFIGURE follow-up logic
 *             can be exercised. Keep aligned with handler.c.
 * */
static void test_process_event(bsp_em7028_handler_t       * const handler,
                               em7028_handler_event_t const * const event)
{
    bsp_em7028_driver_t *drv = handler->p_em7028_instance;

    switch (event->event_type)
    {
    case EM7028_HANDLER_EVENT_START:
        (void)drv->pf_start(drv);
        s_test_is_active = true;
        break;

    case EM7028_HANDLER_EVENT_STOP:
        (void)drv->pf_stop(drv);
        s_test_is_active = false;
        break;

    case EM7028_HANDLER_EVENT_RECONFIGURE:
        (void)drv->pf_apply_config(drv, &event->cfg);
        if (s_test_is_active)
        {
            (void)drv->pf_start(drv);
        }
        break;

    case EM7028_HANDLER_EVENT_DEINIT:
        (void)drv->pf_deinit(drv);
        s_test_is_active = false;
        break;

    default:
        break;
    }
}

/**
 * @brief      Mirror of the sampling block inside the dispatch loop.
 *             Read a frame from the (wrapped) driver and push to the
 *             frame queue. No-op when idle.
 * */
static void test_sample_tick(bsp_em7028_handler_t * const handler)
{
    if (!s_test_is_active)
    {
        return;
    }

    em7028_ppg_frame_t frame = {0};
    em7028_status_t ret = handler->p_em7028_instance->pf_read_frame(
                              handler->p_em7028_instance, &frame);
    if (EM7028_OK == ret)
    {
        (void)handler->p_os_interface->p_os_queue_interface->pf_os_queue_put(
            handler->p_frame_queue, &frame, 0U);
    }
}

/* ======================================================================== */
/*  Pass / fail helpers                                                     */
/* ======================================================================== */
static void case_report(const char *name, bool ok)
{
    if (ok)
    {
        s_pass_count++;
        DEBUG_OUT(d, EM7028_LOG_TAG, "[PASS] %s", name);
    }
    else
    {
        s_fail_count++;
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "[FAIL] %s qc=%u qp=%u qg=%u iic_w=%u iic_r=%u "
                  "drv: init=%u start=%u stop=%u apply=%u rdfrm=%u",
                  name,
                  (unsigned)s_st.queue_create_count,
                  (unsigned)s_st.queue_put_count,
                  (unsigned)s_st.queue_get_count,
                  (unsigned)s_st.iic_write_count,
                  (unsigned)s_st.iic_read_count,
                  (unsigned)s_st.drv_init_count,
                  (unsigned)s_st.drv_start_count,
                  (unsigned)s_st.drv_stop_count,
                  (unsigned)s_st.drv_apply_config_count,
                  (unsigned)s_st.drv_read_frame_count);
    }
}

/* ======================================================================== */
/*  Test cases -- inst error paths (no driver_inst called -- safe to repeat) */
/* ======================================================================== */
static void test_case_inst_null_handler(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 1: inst(NULL, &args) -> ERRORPARAMETER =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();

    em7028_handler_status_t ret = bsp_em7028_handler_inst(NULL, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORPARAMETER == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 1 inst NULL handler", ok);
}

static void test_case_inst_null_args(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 2: inst(&h, NULL) -> ERRORPARAMETER =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();

    em7028_handler_status_t ret = bsp_em7028_handler_inst(&s_mock_handler, NULL);

    bool ok = (EM7028_HANDLER_ERRORPARAMETER == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 2 inst NULL args", ok);
}

static void test_case_inst_null_driver(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 3: handler->p_em7028_instance=NULL -> "
              "ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_handler.p_em7028_instance = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 3 inst NULL driver", ok);
}

static void test_case_inst_null_private(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 4: handler->p_private_data=NULL -> "
              "ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_handler.p_private_data = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 4 inst NULL private", ok);
}

static void test_case_inst_null_os_if(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 5: args.p_os_interface=NULL -> ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_args.p_os_interface = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 5 inst NULL os_if", ok);
}

static void test_case_inst_null_iic(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 6: args.p_iic_interface=NULL -> ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_args.p_iic_interface = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 6 inst NULL iic", ok);
}

static void test_case_inst_null_timebase(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 7: args.p_timebase_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_args.p_timebase_interface = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 7 inst NULL timebase", ok);
}

static void test_case_inst_null_delay(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 8: args.p_delay_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_args.p_delay_interface = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 8 inst NULL delay", ok);
}

static void test_case_inst_null_queue_if(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 9: os_if->p_os_queue_interface=NULL -> "
              "ERRORRESOURCE =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();
    s_mock_os_if.p_os_queue_interface = NULL;

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count);
    case_report("CASE 9 inst NULL queue_if", ok);
}

static void test_case_inst_cmd_queue_create_fails(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 10: cmd queue_create fails -> propagated =====");
    state_reset(); mock_queues_reset();
    s_st.create_fail_on_call = 1U;   /* fail the very first create        */
    s_st.force_create_ret    = EM7028_HANDLER_ERRORNOMEMORY;
    mock_handler_bind();
    mock_handler_instance_init();

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORNOMEMORY == ret) &&
              (1U == s_st.queue_create_count) &&
              (0U == s_st.iic_init_count);
    case_report("CASE 10 inst cmd queue_create fail", ok);
}

static void test_case_inst_frame_queue_create_fails(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 11: frame queue_create fails -> propagated =====");
    state_reset(); mock_queues_reset();
    s_st.create_fail_on_call = 2U;   /* second create -- frame queue      */
    s_st.force_create_ret    = EM7028_HANDLER_ERRORNOMEMORY;
    mock_handler_bind();
    mock_handler_instance_init();

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORNOMEMORY == ret) &&
              (2U == s_st.queue_create_count) &&
              (0U == s_st.iic_init_count);
    case_report("CASE 11 inst frame queue_create fail", ok);
}

/* ======================================================================== */
/*  Test cases -- public APIs reject when no handler is mounted             */
/* ======================================================================== */
static void test_case_start_uninstalled(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 12: start() before mount -> ERRORNOTINIT =====");
    em7028_handler_status_t ret = bsp_em7028_handler_start();
    case_report("CASE 12 start uninstalled",
                EM7028_HANDLER_ERRORNOTINIT == ret);
}

static void test_case_stop_uninstalled(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 13: stop() before mount -> ERRORNOTINIT =====");
    em7028_handler_status_t ret = bsp_em7028_handler_stop();
    case_report("CASE 13 stop uninstalled",
                EM7028_HANDLER_ERRORNOTINIT == ret);
}

static void test_case_reconfigure_null_cfg(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 14: reconfigure(NULL) -> ERRORPARAMETER =====");
    em7028_handler_status_t ret = bsp_em7028_handler_reconfigure(NULL);
    case_report("CASE 14 reconfigure NULL cfg",
                EM7028_HANDLER_ERRORPARAMETER == ret);
}

static void test_case_get_frame_null(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 15: get_frame(NULL, 0) -> ERRORPARAMETER =====");
    em7028_handler_status_t ret = bsp_em7028_handler_get_frame(NULL, 0U);
    case_report("CASE 15 get_frame NULL",
                EM7028_HANDLER_ERRORPARAMETER == ret);
}

static void test_case_get_frame_uninstalled(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 16: get_frame(&f, 0) before mount -> "
              "ERRORNOTINIT =====");
    em7028_ppg_frame_t f;
    em7028_handler_status_t ret = bsp_em7028_handler_get_frame(&f, 0U);
    case_report("CASE 16 get_frame uninstalled",
                EM7028_HANDLER_ERRORNOTINIT == ret);
}

/* ======================================================================== */
/*  Test cases -- successful inst (consumes one driver PIMPL slot)          */
/* ======================================================================== */
static bool g_inst_success = false;   /* shared with downstream cases      */

static void test_case_inst_success(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 17: inst succeeds -- 2 queues, driver auto-init "
              "via I2C =====");
    state_reset(); mock_queues_reset();
    mock_handler_bind();
    mock_handler_instance_init();

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    /* Expected:
     *   - 2 queue creates: cmd (depth=4, size=sizeof(event_t)),
     *                      frame (depth=10, size=sizeof(ppg_frame_t))
     *   - iic_init=1 from driver auto-init
     *   - iic_read>=1 (at least the ID_REG probe)
     *   - iic_write>=6 (SOFT_RESET + 5 cfg writes)                      */
    bool ok = (EM7028_HANDLER_OK == ret) &&
              (2U == s_st.queue_create_count) &&
              (NULL != s_mock_handler.p_cmd_queue) &&
              (NULL != s_mock_handler.p_frame_queue) &&
              (s_mock_handler.p_cmd_queue   == s_st.cmd_queue_handle) &&
              (s_mock_handler.p_frame_queue == s_st.frame_queue_handle) &&
              (sizeof(em7028_ppg_frame_t) == s_st.last_create_item_size) &&
              (1U == s_st.iic_init_count) &&
              (s_st.iic_read_count >= 1U) &&
              (s_st.iic_write_count >= 6U);

    if (ok)
    {
        /* Re-wrap the driver vtable so subsequent dispatch tests stay
         * in counter-only mode and do not re-bang the I2C path.       */
        mock_driver_rewrap(&s_mock_driver);
        g_inst_success = true;
    }
    case_report("CASE 17 inst success", ok);
}

static void test_case_inst_already_inited(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 18: inst again on inited handler -> "
              "ERRORRESOURCE =====");
    if (!g_inst_success)
    {
        case_report("CASE 18 already inited (skipped)", false);
        return;
    }

    state_reset();   /* keep mock_queues / handler intact                  */

    em7028_handler_status_t ret =
        bsp_em7028_handler_inst(&s_mock_handler, &s_mock_args);

    bool ok = (EM7028_HANDLER_ERRORRESOURCE == ret) &&
              (0U == s_st.queue_create_count) &&
              (0U == s_st.iic_init_count);
    case_report("CASE 18 already inited", ok);
}

/* ======================================================================== */
/*  Test cases -- dispatch routing (mirror of process_event)                */
/* ======================================================================== */
static void test_case_dispatch_start(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 19: dispatch START -> drv->pf_start =====");
    if (!g_inst_success) { case_report("CASE 19 (skipped)", false); return; }
    state_reset();
    s_test_is_active = false;

    em7028_handler_event_t event = {.event_type = EM7028_HANDLER_EVENT_START};
    test_process_event(&s_mock_handler, &event);

    bool ok = (1U == s_st.drv_start_count) &&
              (0U == s_st.drv_stop_count) &&
              (0U == s_st.drv_apply_config_count) &&
              (0U == s_st.drv_deinit_count) &&
              (true == s_test_is_active);
    case_report("CASE 19 dispatch START", ok);
}

static void test_case_dispatch_stop(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 20: dispatch STOP -> drv->pf_stop =====");
    if (!g_inst_success) { case_report("CASE 20 (skipped)", false); return; }
    state_reset();
    s_test_is_active = true;

    em7028_handler_event_t event = {.event_type = EM7028_HANDLER_EVENT_STOP};
    test_process_event(&s_mock_handler, &event);

    bool ok = (0U == s_st.drv_start_count) &&
              (1U == s_st.drv_stop_count) &&
              (false == s_test_is_active);
    case_report("CASE 20 dispatch STOP", ok);
}

static void test_case_dispatch_reconfigure_active(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 21: dispatch RECONFIGURE while ACTIVE -> "
              "apply_config + start =====");
    if (!g_inst_success) { case_report("CASE 21 (skipped)", false); return; }
    state_reset();
    s_test_is_active = true;

    em7028_handler_event_t event = {
        .event_type = EM7028_HANDLER_EVENT_RECONFIGURE,
        .cfg = {
            .enable_hrs1     = true,
            .enable_hrs2     = false,
            .hrs1_freq       = EM7028_HRS1_FREQ_327K_12_5MS,
            .hrs1_res        = EM7028_HRS_RES_14BIT,
            .hrs2_mode       = EM7028_HRS2_MODE_PULSE,
            .hrs2_wait       = EM7028_HRS2_WAIT_25MS,
            .hrs2_pos_mask   = 0x0Fu,
            .led_current     = 0x40u,
        },
    };
    test_process_event(&s_mock_handler, &event);

    bool ok = (1U == s_st.drv_apply_config_count) &&
              (1U == s_st.drv_start_count) &&
              (EM7028_HRS1_FREQ_327K_12_5MS == s_st.last_apply_cfg.hrs1_freq) &&
              (0x40u == s_st.last_apply_cfg.led_current);
    case_report("CASE 21 dispatch RECONFIGURE active", ok);
}

static void test_case_dispatch_reconfigure_idle(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 22: dispatch RECONFIGURE while IDLE -> "
              "apply_config only =====");
    if (!g_inst_success) { case_report("CASE 22 (skipped)", false); return; }
    state_reset();
    s_test_is_active = false;

    em7028_handler_event_t event = {
        .event_type = EM7028_HANDLER_EVENT_RECONFIGURE,
    };
    test_process_event(&s_mock_handler, &event);

    bool ok = (1U == s_st.drv_apply_config_count) &&
              (0U == s_st.drv_start_count);
    case_report("CASE 22 dispatch RECONFIGURE idle", ok);
}

static void test_case_dispatch_deinit(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 23: dispatch DEINIT -> drv->pf_deinit =====");
    if (!g_inst_success) { case_report("CASE 23 (skipped)", false); return; }
    state_reset();
    s_test_is_active = true;

    em7028_handler_event_t event = {.event_type = EM7028_HANDLER_EVENT_DEINIT};
    test_process_event(&s_mock_handler, &event);

    bool ok = (1U == s_st.drv_deinit_count) &&
              (false == s_test_is_active);
    case_report("CASE 23 dispatch DEINIT", ok);
}

/* ======================================================================== */
/*  Test cases -- sample tick + frame queue round-trip                      */
/* ======================================================================== */
static void test_case_sample_tick_active(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 24: sample tick (ACTIVE) -> read_frame + "
              "queue_put on frame_queue =====");
    if (!g_inst_success) { case_report("CASE 24 (skipped)", false); return; }
    state_reset();
    s_test_is_active = true;

    test_sample_tick(&s_mock_handler);

    bool ok = (1U == s_st.drv_read_frame_count) &&
              (1U == s_st.queue_put_count);
    case_report("CASE 24 sample tick active", ok);
}

static void test_case_sample_tick_idle(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 25: sample tick (IDLE) -> nothing happens =====");
    if (!g_inst_success) { case_report("CASE 25 (skipped)", false); return; }
    state_reset();
    s_test_is_active = false;

    test_sample_tick(&s_mock_handler);

    bool ok = (0U == s_st.drv_read_frame_count) &&
              (0U == s_st.queue_put_count);
    case_report("CASE 25 sample tick idle", ok);
}

static void test_case_frame_queue_round_trip(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 26: producer push -> consumer get_frame "
              "round-trip =====");
    if (!g_inst_success) { case_report("CASE 26 (skipped)", false); return; }
    state_reset();
    s_test_is_active = true;

    /* Producer side: one tick generates one frame. */
    test_sample_tick(&s_mock_handler);

    /* Consumer side: pull the frame back out of the same queue. */
    em7028_ppg_frame_t pulled = {0};
    em7028_handler_status_t qret =
        s_mock_os_queue.pf_os_queue_get(s_mock_handler.p_frame_queue,
                                        &pulled, 0U);

    bool ok = (1U == s_st.drv_read_frame_count) &&
              (1U == s_st.queue_put_count) &&
              (EM7028_HANDLER_OK == qret) &&
              (pulled.hrs1_pixel[0] != 0U) &&
              (pulled.hrs1_sum != 0U);
    case_report("CASE 26 frame queue round-trip", ok);
}

static void test_case_frame_queue_drop_on_full(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 27: frame queue drop-on-full -- producer wins "
              "10 ticks, 11th rejected =====");
    if (!g_inst_success) { case_report("CASE 27 (skipped)", false); return; }
    state_reset();
    s_test_is_active = true;

    /* Drain any leftover frames from prior cases. */
    em7028_ppg_frame_t drain;
    while (EM7028_HANDLER_OK ==
           s_mock_os_queue.pf_os_queue_get(s_mock_handler.p_frame_queue,
                                            &drain, 0U))
    {
        /* keep draining */
    }
    state_reset();   /* zero the counters AFTER drain                     */

    /* Push EM7028_HDL_MOCK_FRAME_DEPTH frames -- queue should accept all. */
    for (uint32_t i = 0U; i < EM7028_HDL_MOCK_FRAME_DEPTH; i++)
    {
        test_sample_tick(&s_mock_handler);
    }
    const uint32_t puts_before_overflow = s_st.queue_put_count;

    /* One more tick -- queue is full, the producer must NOT block but
     * the put should fail and we just drop the frame.                  */
    test_sample_tick(&s_mock_handler);

    bool ok = (EM7028_HDL_MOCK_FRAME_DEPTH     == puts_before_overflow) &&
              (EM7028_HDL_MOCK_FRAME_DEPTH + 1 == s_st.queue_put_count) &&
              (EM7028_HDL_MOCK_FRAME_DEPTH + 1 == s_st.drv_read_frame_count);
    case_report("CASE 27 frame queue drop-on-full", ok);
}
//******************************* Functions *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Entry point for the EM7028 handler mock test task.
 *
 * @param[in]  argument : Unused task argument.
 *
 * @return     None.
 * */
void em7028_handler_mock_test_task(void *argument)
{
    (void)argument;

    osal_task_delay(EM7028_HDL_MOCK_BOOT_WAIT_MS);
    DEBUG_OUT(d, EM7028_LOG_TAG, "em7028_handler_mock_test_task start");

    /* ---- inst error paths (no driver_inst, no PIMPL slot consumed) ---- */
    test_case_inst_null_handler();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_args();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_driver();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_private();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_os_if();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_iic();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_timebase();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_delay();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_null_queue_if();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_cmd_queue_create_fails();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_frame_queue_create_fails();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);

    /* ---- public API rejection paths (gp_em7028_handler still NULL) ---- */
    test_case_start_uninstalled();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_stop_uninstalled();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_reconfigure_null_cfg();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_get_frame_null();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_get_frame_uninstalled();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);

    /* ---- the ONE successful inst (consumes the only PIMPL slot) ------- */
    test_case_inst_success();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_inst_already_inited();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);

    /* ---- dispatch routing (uses the inited handler + wrapped vtable) -- */
    test_case_dispatch_start();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_dispatch_stop();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_dispatch_reconfigure_active();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_dispatch_reconfigure_idle();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_dispatch_deinit();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);

    /* ---- sampling + frame queue ---- */
    test_case_sample_tick_active();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_sample_tick_idle();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_frame_queue_round_trip();
    osal_task_delay(EM7028_HDL_MOCK_STEP_GAP_MS);
    test_case_frame_queue_drop_on_full();

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== HANDLER SUMMARY: %u PASS / %u FAIL =====",
              (unsigned)s_pass_count, (unsigned)s_fail_count);
    if (0U == s_fail_count)
    {
        DEBUG_OUT(d, EM7028_LOG_TAG,
                  "em7028_handler_mock_test_task ALL PASS");
    }
    else
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028_handler_mock_test_task %u FAIL -- review log",
                  (unsigned)s_fail_count);
    }

    for (;;)
    {
        osal_task_delay(5000U);
    }
}
//******************************* Functions *********************************//
