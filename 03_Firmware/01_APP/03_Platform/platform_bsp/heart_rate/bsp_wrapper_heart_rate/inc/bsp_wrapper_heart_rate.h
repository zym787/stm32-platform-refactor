/******************************************************************************
 * @file bsp_wrapper_heart_rate.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - stddef.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for PPG heart-rate sensor access.
 *        Decouples the application from any specific PPG chip driver.
 *        The adapter layer registers a concrete driver via
 *        drv_adapter_heart_rate_mount(); the application calls the public
 *        API without knowing which hardware is underneath.
 *
 * Processing flow:
 *   1. Adapter calls drv_adapter_heart_rate_mount() to register the vtable.
 *   2. Application calls heart_rate_drv_init() once at startup.
 *   3. Application optionally pushes a fresh wp_heart_rate_config_t through
 *      heart_rate_drv_reconfigure(), then calls heart_rate_drv_start() to
 *      begin streaming.
 *   4. Streaming loop:
 *        heart_rate_drv_get_req(timeout_ms) blocks for the next PPG frame.
 *        heart_rate_get_frame_addr()        returns a pointer to it.
 *        heart_rate_read_data_done()        signals consumption complete.
 *
 * @version V1.0 2026-05-07
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_HEART_RATE_H__
#define __BSP_WRAPPER_HEART_RATE_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Number of pixels per HRS channel that the wrapper exposes. Matches the
 * EM7028 4-pixel layout; future chips with more pixels will need to map
 * down to this width or this constant will be revisited.                */
#define WP_HEART_RATE_PIXEL_NUM         (4U)

/**
 * @brief Return codes for all heart-rate abstraction-layer APIs.
 */
typedef enum
{
    WP_HEART_RATE_OK               = 0,        /* Operation successful       */
    WP_HEART_RATE_ERROR            = 1,        /* General error              */
    WP_HEART_RATE_ERRORTIMEOUT     = 2,        /* Timeout error              */
    WP_HEART_RATE_ERRORRESOURCE    = 3,        /* Resource unavailable       */
    WP_HEART_RATE_ERRORPARAMETER   = 4,        /* Invalid parameter          */
    WP_HEART_RATE_ERRORNOMEMORY    = 5,        /* Out of memory              */
    WP_HEART_RATE_ERRORUNSUPPORTED = 6,        /* Unsupported feature        */
    WP_HEART_RATE_ERRORISR         = 7,        /* ISR context error          */
    WP_HEART_RATE_ERRORNOTINIT     = 8,        /* API used before mount/init */
    WP_HEART_RATE_RESERVED         = 0xFF,     /* Reserved                   */
} wp_heart_rate_status_t;

/* HRS2 work mode -- selects whether HRS2 samples continuously or in
 * pulse mode at WP_HRS2_WAIT_*. Values are kept identical to the
 * underlying chip so adapter translation is a direct cast.            */
typedef enum
{
    WP_HRS2_MODE_CONTINUOUS = 0,               /* HRS2 always sampling       */
    WP_HRS2_MODE_PULSE      = 1,               /* HRS2 pulsed at WP_HRS2_WAIT*/
} wp_hrs2_mode_t;

/* HRS2 sample-period selector (pulse mode). */
typedef enum
{
    WP_HRS2_WAIT_CONTINUOUS = 0x00,            /* 0 (free running)           */
    WP_HRS2_WAIT_1_5625MS   = 0x01,            /* 640 Hz                     */
    WP_HRS2_WAIT_3_125MS    = 0x02,            /* 320 Hz                     */
    WP_HRS2_WAIT_6_25MS     = 0x03,            /* 160 Hz                     */
    WP_HRS2_WAIT_12_5MS     = 0x04,            /*  80 Hz                     */
    WP_HRS2_WAIT_25MS       = 0x05,            /*  40 Hz (project target)    */
    WP_HRS2_WAIT_50MS       = 0x06,            /*  20 Hz                     */
    WP_HRS2_WAIT_100MS      = 0x07,            /*  10 Hz                     */
} wp_hrs2_wait_t;

/* HRS1 ADC integration frequency / max effective sample rate. */
typedef enum
{
    WP_HRS1_FREQ_2_62MHZ_1_5625MS = 0x00,      /* 640 Hz max                 */
    WP_HRS1_FREQ_1_31MHZ_3_125MS  = 0x01,      /* 320 Hz max                 */
    WP_HRS1_FREQ_655K_6_25MS      = 0x02,      /* 160 Hz max                 */
    WP_HRS1_FREQ_327K_12_5MS      = 0x03,      /*  80 Hz max                 */
    WP_HRS1_FREQ_163K_25MS        = 0x04,      /*  40 Hz (project target)    */
    WP_HRS1_FREQ_81K_50MS         = 0x05,      /*  20 Hz max                 */
    WP_HRS1_FREQ_40K_100MS        = 0x06,      /*  10 Hz                     */
    WP_HRS1_FREQ_20K_200MS        = 0x07,      /*   5 Hz max                 */
} wp_hrs1_freq_t;

/* HRS ADC resolution. */
typedef enum
{
    WP_HRS_RES_10BIT = 0x00,
    WP_HRS_RES_12BIT = 0x01,
    WP_HRS_RES_14BIT = 0x02,                   /* project default            */
    WP_HRS_RES_16BIT = 0x03,
} wp_hrs_res_t;

/**
 * @brief One PPG frame surfaced by the wrapper -- 4 pixels per channel
 *        plus a pre-summed accumulator the algorithm layer can feed
 *        straight into a band-pass filter.
 */
typedef struct
{
    uint32_t timestamp_ms;                          /* tick at read         */
    uint16_t hrs1_pixel[WP_HEART_RATE_PIXEL_NUM];   /* HRS1 raw pixels      */
    uint16_t hrs2_pixel[WP_HEART_RATE_PIXEL_NUM];   /* HRS2 raw pixels      */
    uint32_t hrs1_sum;                              /* sum of HRS1 pixels   */
    uint32_t hrs2_sum;                              /* sum of HRS2 pixels   */
} wp_ppg_frame_t;

/**
 * @brief Sensor parameters applied in one shot by heart_rate_drv_reconfigure.
 *        Field semantics mirror the EM7028 configuration; future chip
 *        adapters are responsible for translating to their own register
 *        layout.
 */
typedef struct
{
    /* Channel enables. */
    bool             enable_hrs1;
    bool             enable_hrs2;

    /* HRS1 channel parameters. */
    bool             hrs1_gain_high;            /* false=x1  true=x5         */
    bool             hrs1_range_high;           /* false=x1  true=x8         */
    wp_hrs1_freq_t   hrs1_freq;
    wp_hrs_res_t     hrs1_res;

    /* HRS2 channel parameters. */
    wp_hrs2_mode_t   hrs2_mode;
    wp_hrs2_wait_t   hrs2_wait;
    bool             hrs2_gain_high;            /* false=x1  true=x10        */
    uint8_t          hrs2_pos_mask;             /* 7-bit pixel mask          */

    /* Common LED current (chip-specific scaling). */
    uint8_t          led_current;
} wp_heart_rate_config_t;

/**
 * @brief Driver vtable for a PPG heart-rate sensor.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _heart_rate_drv_t
{
    uint8_t                       idx;          /* Slot index in the table  */
    uint32_t                   dev_id;          /* Hardware device id       */
    void *                  user_data;          /* Adapter private context  */

    void (*pf_heart_rate_drv_init  )(struct _heart_rate_drv_t *const dev);
    void (*pf_heart_rate_drv_deinit)(struct _heart_rate_drv_t *const dev);

    /* Lifecycle control. */
    wp_heart_rate_status_t (*pf_heart_rate_drv_start      )(
                              struct _heart_rate_drv_t *const dev);
    wp_heart_rate_status_t (*pf_heart_rate_drv_stop       )(
                              struct _heart_rate_drv_t *const dev);
    wp_heart_rate_status_t (*pf_heart_rate_drv_reconfigure)(
                              struct _heart_rate_drv_t *const          dev,
                              wp_heart_rate_config_t   const *const  p_cfg);

    /* Streaming consumer side. */
    wp_heart_rate_status_t (*pf_heart_rate_drv_get_req)(
                              struct _heart_rate_drv_t *const dev,
                              uint32_t                 timeout_ms);
    wp_ppg_frame_t       * (*pf_heart_rate_get_frame_addr)(
                              struct _heart_rate_drv_t *const dev);
    void                   (*pf_heart_rate_read_data_done)(
                              struct _heart_rate_drv_t *const dev);
} heart_rate_drv_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
 * @brief   Register a heart-rate driver into the wrapper slot table.
 *          Called exclusively by the adapter layer at system init.
 *
 * @param[in] idx : Slot index (0 ~ HEART_RATE_DRV_MAX_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
bool drv_adapter_heart_rate_mount(uint8_t idx, heart_rate_drv_t *const drv);

/**
 * @brief   Initialize the currently active heart-rate sensor driver.
 *          Forwards to pf_heart_rate_drv_init in the registered vtable.
 */
void heart_rate_drv_init(void);

/**
 * @brief   Deinitialize the currently active heart-rate sensor driver.
 */
void heart_rate_drv_deinit(void);

/**
 * @brief   Begin periodic PPG sampling on the underlying handler.
 *
 * @return  WP_HEART_RATE_OK on success; WP_HEART_RATE_ERRORRESOURCE if no
 *          driver is mounted; otherwise the chip-specific error code.
 */
wp_heart_rate_status_t heart_rate_drv_start(void);

/**
 * @brief   Halt periodic PPG sampling. Cached configuration is preserved.
 */
wp_heart_rate_status_t heart_rate_drv_stop(void);

/**
 * @brief   Apply a new sensor configuration. Safe to call before or after
 *          start; if currently running the underlying handler will pause,
 *          push the new cfg, and resume.
 *
 * @param[in] p_cfg : Configuration to apply (deep-copied internally).
 */
wp_heart_rate_status_t heart_rate_drv_reconfigure(
                                  wp_heart_rate_config_t const *const p_cfg);

/**
 * @brief   Block until the next PPG frame is available, or timeout.
 *
 * @param[in] timeout_ms : Maximum wait in milliseconds.
 *
 * @return  WP_HEART_RATE_OK         - A new frame is ready;
 *                                     read it via heart_rate_get_frame_addr.
 *          WP_HEART_RATE_ERRORTIMEOUT - No frame within timeout.
 *          WP_HEART_RATE_ERRORRESOURCE - Driver not mounted.
 */
wp_heart_rate_status_t heart_rate_drv_get_req(uint32_t timeout_ms);

/**
 * @brief   Address of the most recently delivered frame. Must be called
 *          after heart_rate_drv_get_req() returned WP_HEART_RATE_OK.
 *
 * @return  Pointer to the latest wp_ppg_frame_t, or NULL if no driver is
 *          mounted.
 */
wp_ppg_frame_t *heart_rate_get_frame_addr(void);

/**
 * @brief   Signal that the caller has consumed the current frame.
 *          The wrapper-level implementation is a no-op for queue-pop
 *          backends (the frame is already copied into a private buffer)
 *          but future ring-buffer adapters may use it for release
 *          semantics; callers should still invoke it for symmetry.
 */
void heart_rate_read_data_done(void);
//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_HEART_RATE_H__ */
