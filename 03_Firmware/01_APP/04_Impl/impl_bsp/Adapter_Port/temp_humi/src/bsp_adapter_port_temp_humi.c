/******************************************************************************
 * @file bsp_adapter_port_temp_humi.c
 *
 * @par dependencies
 * - bsp_temp_humi_xxx_handler.h
 * - bsp_adapter_port_temp_humi.h
 * - bsp_wrapper_temp_humi.h
 * - osal_wrapper_adapter.h
 * - osal_error.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port for AHT21 temperature/humidity sensor.
 *
 * Provides concrete implementations for bsp_wrapper_temp_humi's vtable.
 *
 * == Synchronous read flow ==
 *
 *   caller ─► temp_humi_read_all_sync()
 *                ├─ osal_mutex_take()            ── serialise concurrent
 * callers ├─ assign req_id, clear EG bits ├─ bsp_temp_humi_xxx_read()     ──
 * post event (req_id in p_user_ctx) └─ osal_event_group_wait_bits() ── block
 * caller ↑ handler reads AHT21 → aht21_data_ready_cb(temp, humi, ctx) ctx ==
 * (void*)(uintptr_t)req_id → verify req_id == s_inflight_req_id  (discard if
 * stale) → store temp/humi into s_temp_result/s_humi_result →
 * osal_event_group_set_bits()  ── unblock caller caller ◄── mutex released;
 * *temp / *humi filled; returns wp_temp_humi_status_t
 *
 * == Concurrency guarantee ==
 *
 *   s_read_mutex ensures only one _sync call is in flight at any time.
 *   Even so, a call that times out releases the mutex and lets the next
 *   request begin.  The request-ID mechanism prevents a late-arriving
 *   callback from that timed-out request from polluting the new one:
 *
 *     callback stores (void*)(uintptr_t)req_id at dispatch time.
 *     aht21_data_ready_cb() compares the embedded id against s_inflight_req_id.
 *     If they differ the callback is silently discarded.
 *
 * == Resource initialisation ==
 *
 *   adapter_resources_init() is called from aht21_drv_init(), which runs
 *   once during the handler-thread startup phase before any concurrent _sync
 *   calls can arrive.  This avoids a race on lazy first-call initialisation.
 *
 * == Asynchronous read flow ==
 *
 *   aht21_read_*_async() queues an event that carries the user-supplied
 *   callback and user_ctx.  The handler invokes
 *   callback(temp, humi, user_ctx) directly — no mutex or event group used.
 *
 * @version V1.0 2026-04-12
 * @version V2.0 2026-04-12
 * @upgrade 2.0: Event-group sync added; sync/async API split.
 * @version V3.0 2026-04-12
 * @upgrade 3.0: s_read_mutex serialises concurrent _sync calls; request-ID
 *               binding prevents stale-callback pollution after timeout;
 *               sync vtable slots return wp_temp_humi_status_t; async slots
 *               accept user_ctx; all paths emit DEBUG_OUT diagnostics.
 * @version V4.0 2026-04-22
 * @upgrade 4.0: life_time parameter added to all read APIs; forwarded to
 *               handler event to enable per-call data-cache TTL control.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_temp_humi_xxx_handler.h"
#include "bsp_adapter_port_temp_humi.h"
#include "bsp_wrapper_temp_humi.h"
#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/** Maximum ticks to wait for the handler to complete a read. */
#define ADAPTER_EG_READ_TIMEOUT_TICKS (5000U)

/** Event group bit: temperature result ready. */
#define ADAPTER_EG_BIT_TEMP           (1U << 0)
/** Event group bit: humidity result ready. */
#define ADAPTER_EG_BIT_HUMI           (1U << 1)
/** Combined mask for both bits. */
#define ADAPTER_EG_BIT_BOTH           (ADAPTER_EG_BIT_TEMP | ADAPTER_EG_BIT_HUMI)

/** req_id value used to mark "no request in flight". */
#define ADAPTER_REQ_ID_NONE           (0U)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static void aht21_drv_init(temp_humi_drv_t *const dev);
static void aht21_drv_deinit(temp_humi_drv_t *const dev);

static wp_temp_humi_status_t aht21_read_temp_sync(temp_humi_drv_t *const  dev,
                                                            float *const temp,
                                                         uint32_t   life_time);
static wp_temp_humi_status_t aht21_read_humi_sync(temp_humi_drv_t *const  dev,
                                                            float *const humi,
                                                         uint32_t   life_time);
static wp_temp_humi_status_t aht21_read_all_sync(temp_humi_drv_t *const   dev,
                                                           float *const  temp,
                                                           float *const  humi,
                                                        uint32_t    life_time);
static void                  aht21_read_temp_async(temp_humi_drv_t *const dev,
                                                   temp_humi_cb_async_t    cb,
                                                   uint32_t         life_time);
static void                  aht21_read_humi_async(temp_humi_drv_t *const dev,
                                                   temp_humi_cb_async_t    cb,
                                                   uint32_t         life_time);
static void                  aht21_read_all_async (temp_humi_drv_t *const dev,
                                                   temp_humi_cb_async_t    cb,
                                                   uint32_t         life_time);

static void aht21_data_ready_cb(float *temp, float *humi, void *ctx);
//******************************* Declaring *********************************//

//******************************** Variables ********************************//
/**
 * Result storage written by aht21_data_ready_cb() (handler thread context)
 * and read back by the _sync caller after the event group unblocks it.
 * volatile: prevent compiler from caching across context switches.
 */
static volatile float s_temp_result          = 0.0f;
static volatile float s_humi_result          = 0.0f;

/** Event group signalled by aht21_data_ready_cb(). */
static osal_event_group_handle_t s_eg_handle = NULL;

/** Mutex: at most one _sync call executes at a time. */
static osal_mutex_handle_t s_read_mutex      = NULL;

/**
 * Monotonically increasing request counter.  Each _sync call increments this
 * and stores the value in s_inflight_req_id before posting to the queue.
 * Never wraps to ADAPTER_REQ_ID_NONE (0) — the ++ brings 0xFF... to 1.
 */
static volatile uint32_t s_req_id_counter    = 0U;

/**
 * req_id of the currently awaited request.
 * Written by the task holding s_read_mutex; read by aht21_data_ready_cb()
 * (handler thread).  32-bit aligned — Cortex-M4 single-cycle atomic read.
 */
static volatile uint32_t s_inflight_req_id   = ADAPTER_REQ_ID_NONE;

/**
 * Event type of the in-flight request; used by aht21_data_ready_cb() to
 * determine which event-group bits to set.
 * Written by task under mutex before queue post; read by callback after
 * queue receive — the queue acts as the memory barrier.
 */
static volatile temp_humi_data_type_event_t s_inflight_event_type =
    TEMP_HUMI_EVENT_BOTH;
//******************************** Variables ********************************//

//******************************* Functions *********************************//

/* ---------- Internal helpers ---------------------------------------------- */

/**
 * @brief Create the event group and mutex (idempotent — skips if already done).
 *
 * Called from aht21_drv_init() during single-threaded startup, so no locking
 * is required here.
 *
 * @return true on success or if resources already exist.
 */
static bool adapter_resources_init(void)
{
    if (NULL == s_eg_handle)
    {
        int32_t ret = osal_event_group_create(&s_eg_handle);
        if (OSAL_SUCCESS != ret)
        {
            DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                      "adapter_init: event group create failed (%d)", (int)ret);
            return false;
        }
        DEBUG_OUT(i, TEMP_HUMI_LOG_TAG, "adapter_init: event group created");
    }

    if (NULL == s_read_mutex)
    {
        int32_t ret = osal_mutex_init(&s_read_mutex);
        if (OSAL_SUCCESS != ret)
        {
            DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                      "adapter_init: mutex init failed (%d)", (int)ret);
            return false;
        }
        DEBUG_OUT(i, TEMP_HUMI_LOG_TAG, "adapter_init: mutex created");
    }

    return true;
}

/**
 * @brief Submit a read event and block the caller until data is ready.
 *
 * Protocol:
 *  1. Acquire s_read_mutex  (serialises concurrent callers).
 *  2. Bump req_id counter; publish new id to s_inflight_req_id.
 *  3. Record event type in s_inflight_event_type.
 *  4. Clear stale event-group bits from a previous timed-out request.
 *  5. Post the event (req_id encoded in p_user_ctx).
 *  6. Wait on the event group.
 *  7. On timeout: invalidate s_inflight_req_id so any late callback discards.
 *  8. Release s_read_mutex.
 *
 * @param event_type Which axis to read.
 * @param wait_bits  Event-group bits that signal completion.
 * @param life_time  Maximum age of cached data in ms; 0 forces a fresh read.
 * @return WP_TEMP_HUMI_OK, WP_TEMP_HUMI_ERRORTIMEOUT, or
 * WP_TEMP_HUMI_ERRORRESOURCE.
 */
static wp_temp_humi_status_t adapter_sync_read(temp_humi_data_type_event_t event_type,
                                                                   uint32_t wait_bits,
                                                                   uint32_t life_time)
{
    if (NULL == s_eg_handle || NULL == s_read_mutex)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                  "adapter_sync_read: resources not initialised");
        return WP_TEMP_HUMI_ERRORRESOURCE;
    }

    /* ---- 1. Acquire mutex ---- */
    int32_t lock_ret = osal_mutex_take(s_read_mutex, OSAL_MAX_DELAY);
    if (OSAL_SUCCESS != lock_ret)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                  "adapter_sync_read: mutex take failed (%d)", (int)lock_ret);
        return WP_TEMP_HUMI_ERRORTIMEOUT;
    }

    /* ---- 2. Assign a fresh request ID ---- */
    uint32_t this_req_id = ++s_req_id_counter;
    if (ADAPTER_REQ_ID_NONE == this_req_id)
    {
        this_req_id = ++s_req_id_counter; /* skip the reserved sentinel      */
    }
    s_inflight_req_id     = this_req_id;
    s_inflight_event_type = event_type;

    /* ---- 3. Clear stale bits ---- */
    (void)osal_event_group_clear_bits(s_eg_handle, ADAPTER_EG_BIT_BOTH);

    DEBUG_OUT(i, TEMP_HUMI_LOG_TAG,
              "adapter_sync_read: req_id=%u type=%d posting",
              (unsigned)this_req_id, (int)event_type);

    /* ---- 4. Post request to the handler queue ---- */
    temp_humi_xxx_event_t event = {
        .event_type  = event_type,
        .lifetime    = life_time,
        .p_user_ctx  = (void *)(uintptr_t)this_req_id,
        .pf_callback = aht21_data_ready_cb,
    };

    temp_humi_status_t post_ret = bsp_temp_humi_xxx_read(&event);
    if (TEMP_HUMI_OK != post_ret)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                  "adapter_sync_read: queue post failed (%d)", (int)post_ret);
        s_inflight_req_id = ADAPTER_REQ_ID_NONE;
        (void)osal_mutex_give(s_read_mutex);
        return (wp_temp_humi_status_t)post_ret;
    }

    /* ---- 5. Wait for the callback to signal completion ---- */
    bool wait_all  = (ADAPTER_EG_BIT_BOTH == wait_bits);

    int32_t eg_ret = osal_event_group_wait_bits(
        s_eg_handle, wait_bits, true, /* clear bits on exit   */
        wait_all,                     /* AND for BOTH, OR for single-axis */
        ADAPTER_EG_READ_TIMEOUT_TICKS, NULL);

    wp_temp_humi_status_t result;
    if (OSAL_SUCCESS != eg_ret)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                  "adapter_sync_read: req_id=%u wait timeout (osal=%d)",
                  (unsigned)this_req_id, (int)eg_ret);
        /*
         * Invalidate the slot: any callback that arrives late will see
         * s_inflight_req_id == ADAPTER_REQ_ID_NONE != its own id and discard.
         */
        s_inflight_req_id = ADAPTER_REQ_ID_NONE;
        result            = WP_TEMP_HUMI_ERRORTIMEOUT;
    }
    else
    {
        DEBUG_OUT(i, TEMP_HUMI_LOG_TAG,
                  "adapter_sync_read: req_id=%u complete "
                  "temp=%.2f humi=%.2f",
                  (unsigned)this_req_id, (double)s_temp_result,
                  (double)s_humi_result);
        result = WP_TEMP_HUMI_OK;
    }

    /* ---- 6. Release mutex ---- */
    (void)osal_mutex_give(s_read_mutex);
    return result;
}

/**
 * @brief Internal callback used by _sync reads.
 *
 * Runs in the handler task context.
 *
 * @p ctx carries the req_id encoded as (void*)(uintptr_t)req_id at dispatch
 * time.  It is compared against s_inflight_req_id:
 *   - Match: store results, set event-group bits to unblock the caller.
 *   - Mismatch: this is a stale callback from a timed-out request; discard.
 */
static void aht21_data_ready_cb(float *temp, float *humi, void *ctx)
{
    uint32_t incoming_req_id = (uint32_t)(uintptr_t)ctx;

    /* ---- Validate request ID ---- */
    if (incoming_req_id != s_inflight_req_id)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG,
                  "adapter_cb: stale req_id=%u (inflight=%u) — discarded",
                  (unsigned)incoming_req_id, (unsigned)s_inflight_req_id);
        return;
    }

    /* ---- Store sensor values ---- */
    if (NULL != temp)
    {
        s_temp_result = *temp;
    }
    if (NULL != humi)
    {
        s_humi_result = *humi;
    }

    /* ---- Determine bits to set from the in-flight event type ---- */
    uint32_t bits;
    switch (s_inflight_event_type)
    {
    case TEMP_HUMI_EVENT_TEMP:
        bits = ADAPTER_EG_BIT_TEMP;
        break;
    case TEMP_HUMI_EVENT_HUMI:
        bits = ADAPTER_EG_BIT_HUMI;
        break;
    case TEMP_HUMI_EVENT_BOTH:
    default:
        bits = ADAPTER_EG_BIT_BOTH;
        break;
    }

    DEBUG_OUT(i, TEMP_HUMI_LOG_TAG,
              "adapter_cb: req_id=%u temp=%.2f humi=%.2f bits=0x%x",
              (unsigned)incoming_req_id, (double)s_temp_result,
              (double)s_humi_result, (unsigned)bits);

    if (NULL != s_eg_handle)
    {
        (void)osal_event_group_set_bits(s_eg_handle, bits);
    }
}

/* ---------- Wrapper vtable implementations -------------------------------- */

bool drv_adapter_temp_humi_register(void)
{
    temp_humi_drv_t temp_humi_drv = {
        .idx                          = 0,
        .dev_id                       = 0,
        .pf_temp_humi_drv_init        = aht21_drv_init,
        .pf_temp_humi_drv_deinit      = aht21_drv_deinit,
        .pf_temp_humi_read_temp_sync  = aht21_read_temp_sync,
        .pf_temp_humi_read_humi_sync  = aht21_read_humi_sync,
        .pf_temp_humi_read_all_sync   = aht21_read_all_sync,
        .pf_temp_humi_read_temp_async = aht21_read_temp_async,
        .pf_temp_humi_read_humi_async = aht21_read_humi_async,
        .pf_temp_humi_read_all_async  = aht21_read_all_async,
    };

    return drv_adapter_temp_humi_mount(0, &temp_humi_drv);
}

static void aht21_drv_init(temp_humi_drv_t *const dev)
{
    (void)dev;
    (void)adapter_resources_init();
}

static void aht21_drv_deinit(temp_humi_drv_t *const dev)
{
    (void)dev;
}

/* --- Synchronous vtable slots --- */

static wp_temp_humi_status_t aht21_read_temp_sync(temp_humi_drv_t *const dev,
                                                  float *const           temp,
                                                  uint32_t life_time)
{
    (void)dev;
    wp_temp_humi_status_t ret =
        adapter_sync_read(TEMP_HUMI_EVENT_TEMP, ADAPTER_EG_BIT_TEMP, life_time);
    if (WP_TEMP_HUMI_OK == ret && NULL != temp)
    {
        *temp = s_temp_result;
    }
    return ret;
}

static wp_temp_humi_status_t aht21_read_humi_sync(temp_humi_drv_t *const dev,
                                                  float *const           humi,
                                                  uint32_t life_time)
{
    (void)dev;
    wp_temp_humi_status_t ret =
        adapter_sync_read(TEMP_HUMI_EVENT_HUMI, ADAPTER_EG_BIT_HUMI, life_time);
    if (WP_TEMP_HUMI_OK == ret && NULL != humi)
    {
        *humi = s_humi_result;
    }
    return ret;
}

static wp_temp_humi_status_t aht21_read_all_sync(temp_humi_drv_t *const dev,
                                                 float *const           temp,
                                                 float *const           humi,
                                                 uint32_t          life_time)
{
    (void)dev;
    wp_temp_humi_status_t ret =
        adapter_sync_read(TEMP_HUMI_EVENT_BOTH, ADAPTER_EG_BIT_BOTH, life_time);
    if (WP_TEMP_HUMI_OK == ret)
    {
        if (NULL != temp)
        {
            *temp = s_temp_result;
        }
        if (NULL != humi)
        {
            *humi = s_humi_result;
        }
    }
    return ret;
}

/* --- Asynchronous vtable slots --- */
static void async_cb_trampoline(float *temp, float *humi, void *user_ctx)
{
    temp_humi_cb_async_t user_cb = (temp_humi_cb_async_t)user_ctx;
    if (NULL != user_cb)
    {
        user_cb(temp, humi);
    }
}


static void aht21_read_temp_async(temp_humi_drv_t *const dev,
                                  temp_humi_cb_async_t    cb,
                                          uint32_t life_time)
{
    (void)dev;
    if (NULL == cb)
    {
        return;
    }
    temp_humi_xxx_event_t event = {
        .event_type  = TEMP_HUMI_EVENT_TEMP,
        .lifetime    = life_time,
        .p_user_ctx  = (void *)cb,
        .pf_callback = async_cb_trampoline,
    };
    (void)bsp_temp_humi_xxx_read(&event);
}

static void aht21_read_humi_async(temp_humi_drv_t *const dev,
                                  temp_humi_cb_async_t    cb,
                                          uint32_t life_time)
{
    (void)dev;
    if (NULL == cb)
    {
        return;
    }
    temp_humi_xxx_event_t event = {
        .event_type  = TEMP_HUMI_EVENT_HUMI,
        .lifetime    = life_time,
        .p_user_ctx  = (void *)cb,
        .pf_callback = async_cb_trampoline,
    };
    (void)bsp_temp_humi_xxx_read(&event);
}

static void aht21_read_all_async(temp_humi_drv_t *const dev,
                                 temp_humi_cb_async_t     cb,
                                          uint32_t life_time)
{
    (void)dev;
    if (NULL == cb)
    {
        return;
    }
    temp_humi_xxx_event_t event = {
        .event_type  = TEMP_HUMI_EVENT_BOTH,
        .lifetime    = life_time,
        .p_user_ctx  = (void *)cb,
        .pf_callback = async_cb_trampoline,
    };
    (void)bsp_temp_humi_xxx_read(&event);
}

//******************************* Functions *********************************//
