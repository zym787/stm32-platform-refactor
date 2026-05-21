/******************************************************************************
 * @file bsp_em7028_driver.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - bsp_em7028_reg.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 PPG heart-rate sensor driver public interface.
 *        Vtable plus opaque PIMPL state. Hardware and OS dependencies
 *        are injected via iic / delay / os_delay / timebase interface
 *        structs, so the driver stays portable across MCU and RTOS
 *        choices and the upper sensor pipeline (handler -> service)
 *        can drive 40 Hz pulse-mode sampling without touching any
 *        register from the application layer.
 *
 * @version V1.0 2026-04-29
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __BSP_EM7028_DRIVER_H__
#define __BSP_EM7028_DRIVER_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>

#include "bsp_em7028_reg.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* HRSx data registers expose 4 pixels per channel (DATA0..DATA3). */
#define EM7028_HRS_PIXEL_NUM            (4U)

typedef enum
{
    EM7028_OK               = 0,       /* Operation successful               */
    EM7028_ERROR            = 1,       /* General error                      */
    EM7028_ERRORTIMEOUT     = 2,       /* Timeout error                      */
    EM7028_ERRORRESOURCE    = 3,       /* Resource unavailable               */
    EM7028_ERRORPARAMETER   = 4,       /* Invalid parameter                  */
    EM7028_ERRORNOMEMORY    = 5,       /* Out of memory                      */
    EM7028_ERRORUNSUPPORTED = 6,       /* Unsupported feature                */
    EM7028_ERRORISR         = 7,       /* ISR context error                  */
    EM7028_RESERVED         = 0xFF,    /* EM7028 Reserved                    */
} em7028_status_t;

/* HRS2 work mode -- HRS2_CTRL (0x09) bit 7. HRS1 has no equivalent bit. */
typedef enum
{
    EM7028_HRS2_MODE_CONTINUOUS = 0,   /* HRS2 always sampling                */
    EM7028_HRS2_MODE_PULSE      = 1,   /* HRS2 pulsed at HRS2_WAIT_TIME       */
} em7028_hrs2_mode_t;

/* HRS2 sample-period selector -- HRS2_CTRL[6:4]. Pulse mode only. */
typedef enum
{
    EM7028_HRS2_WAIT_CONTINUOUS = 0x00, /* 000 -- 0 (free running)            */
    EM7028_HRS2_WAIT_1_5625MS   = 0x01, /* 001 -- 640 Hz                      */
    EM7028_HRS2_WAIT_3_125MS    = 0x02, /* 010 -- 320 Hz                      */
    EM7028_HRS2_WAIT_6_25MS     = 0x03, /* 011 -- 160 Hz                      */
    EM7028_HRS2_WAIT_12_5MS     = 0x04, /* 100 --  80 Hz (chip default)       */
    EM7028_HRS2_WAIT_25MS       = 0x05, /* 101 --  40 Hz (project target)     */
    EM7028_HRS2_WAIT_50MS       = 0x06, /* 110 --  20 Hz                      */
    EM7028_HRS2_WAIT_100MS      = 0x07, /* 111 --  10 Hz                      */
} em7028_hrs2_wait_t;

/* HRS1 ADC integration frequency -- HRS1_CTRL[5:3]. The numbers in the
 * comment after each entry are the per-sample integration time, which is
 * the maximum effective HRS1 sample rate this clock can support.
 * Note: HRS1 encoding is OFFSET-BY-ONE relative to HRS2 wait_time --
 * HRS1 has no "0/continuous" code; index 000 already maps to 1.5625 ms. */
typedef enum
{
    EM7028_HRS1_FREQ_2_62MHZ_1_5625MS = 0x00, /* 2.62  MHz, 640 Hz max        */
    EM7028_HRS1_FREQ_1_31MHZ_3_125MS  = 0x01, /* 1.31  MHz, 320 Hz max        */
    EM7028_HRS1_FREQ_655K_6_25MS      = 0x02, /* 655.3 kHz, 160 Hz max        */
    EM7028_HRS1_FREQ_327K_12_5MS      = 0x03, /* 327.7 kHz,  80 Hz max        */
    EM7028_HRS1_FREQ_163K_25MS        = 0x04, /* 163.8 kHz,  40 Hz (target)   */
    EM7028_HRS1_FREQ_81K_50MS         = 0x05, /*  81.9 kHz,  20 Hz max        */
    EM7028_HRS1_FREQ_40K_100MS        = 0x06, /*  41.0 kHz,  10 Hz (default)  */
    EM7028_HRS1_FREQ_20K_200MS        = 0x07, /*  20.5 kHz,   5 Hz max        */
} em7028_hrs1_freq_t;

/* HRS ADC resolution -- HRS1_CTRL[2:1]. */
typedef enum
{
    EM7028_HRS_RES_10BIT = 0x00,
    EM7028_HRS_RES_12BIT = 0x01,       /* chip default                        */
    EM7028_HRS_RES_14BIT = 0x02,       /* project default -- best SNR/speed   */
    EM7028_HRS_RES_16BIT = 0x03,
} em7028_hrs_res_t;

/* One PPG frame -- 4 pixels per channel plus a pre-summed accumulator
 * the algorithm layer can feed straight into the band-pass filter.    */
typedef struct
{
    uint32_t timestamp_ms;                       /* tick at read       */
    uint16_t hrs1_pixel[EM7028_HRS_PIXEL_NUM];   /* 0x28..0x2F         */
    uint16_t hrs2_pixel[EM7028_HRS_PIXEL_NUM];   /* 0x20..0x27         */
    uint32_t hrs1_sum;                           /* sum of 4 pixels    */
    uint32_t hrs2_sum;
} em7028_ppg_frame_t;

/* Sensor parameters applied in one shot by pf_apply_config.
 *
 * IR_MODE bit (HRS1_CTRL[0]) is intentionally NOT exposed here; the
 * driver always sets it to 1 (HRS1 / heart-rate mode). The IR mode is
 * a different sensing element that this driver does not service.
 *
 * LED_WIDTH (HRS2_CTRL[3:2]) and LED_CNT (HRS2_CTRL[1:0]) are also not
 * exposed; the driver applies sensible defaults. Use pf_write_reg if
 * you need to override them.                                          */
typedef struct
{
    /* Channel enables -- HRS_CFG (0x01) bit 7 / bit 3                 */
    bool                enable_hrs1;
    bool                enable_hrs2;

    /* HRS1 channel parameters -- HRS1_CTRL (0x0D)                     */
    bool                hrs1_gain_high;   /* HRS_GAIN:  false=x1 true=x5 */
    bool                hrs1_range_high;  /* HRS_RANGE: false=x1 true=x8 */
    em7028_hrs1_freq_t  hrs1_freq;        /* HRS_FREQ[5:3]               */
    em7028_hrs_res_t    hrs1_res;         /* HRS_RES[2:1]                */

    /* HRS2 channel parameters -- HRS2_CTRL (0x09) + HRS2_GAIN_CTRL    */
    em7028_hrs2_mode_t  hrs2_mode;        /* HRS2_CTRL[7]                */
    em7028_hrs2_wait_t  hrs2_wait;        /* HRS2_CTRL[6:4]              */
    bool                hrs2_gain_high;   /* HRS2_GAIN: false=x1 true=x10*/
    uint8_t             hrs2_pos_mask;    /* HRS2_POS[6:0] pixel mask    */

    /* Common -- LED current driver setting -- LED_CRT (0x07)          */
    uint8_t             led_current;
} em7028_config_t;

/*============================ Injected Interfaces ==========================*/

/* I2C bus interface, supplied by the integration layer.                */
typedef struct
{
    void *hi2c;

    em7028_status_t (*pf_iic_init     )(void *);
    em7028_status_t (*pf_iic_deinit   )(void *);

    em7028_status_t (*pf_iic_mem_write)(void    *     i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t *  p_data,
                                        uint16_t     size,
                                        uint32_t  timeout);

    em7028_status_t (*pf_iic_mem_read )(void    *     i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t *  p_data,
                                        uint16_t     size,
                                        uint32_t  timeout);
} em7028_iic_driver_interface_t;

/* Monotonic millisecond tick -- used for timeouts and frame stamps.    */
typedef struct
{
    uint32_t (*pf_get_tick_count)(void);
} em7028_timebase_interface_t;

/* Hardware-grade blocking delay (DWT) -- needed for power-up and
 * soft-reset timing where us-precision matters.                        */
typedef struct
{
    void (*pf_delay_init)(void);
    void (*pf_delay_ms  )(uint32_t const ms);
    void (*pf_delay_us  )(uint32_t const us);
} em7028_delay_interface_t;

/* OS-yield delay -- used for ms-scale waits inside thread context so
 * other tasks can run instead of busy-waiting.                         */
typedef struct
{
    void (*pf_rtos_delay)(uint32_t const ms);
} em7028_os_delay_interface_t;

/*============================ Driver Main Object ===========================*/

/* Opaque private state. Definition lives in bsp_em7028_driver.c so that
 * is_init / cached cfg / counters cannot be touched from outside.       */
struct em7028_private;

typedef struct bsp_em7028_driver
{
    /* PIMPL handle -- allocated by bsp_em7028_driver_inst from a static
     * pool; never dereferenced by upper layers.                        */
    struct em7028_private               *           priv;

    em7028_iic_driver_interface_t const *           p_iic;
    em7028_timebase_interface_t   const *           p_timebase;
    em7028_delay_interface_t      const *           p_delay;
    em7028_os_delay_interface_t   const *           p_os_delay;

    /* Bring chip out of reset and program the cached cfg into hardware.*/
    em7028_status_t (*pf_init        )(struct bsp_em7028_driver *const self);

    /* Disable both channels and release the I2C transport. Idempotent. */
    em7028_status_t (*pf_deinit      )(struct bsp_em7028_driver *const self);

    /* Issue SOFT_RESET (0x0F) and wait for power-on default state.     */
    em7028_status_t (*pf_soft_reset  )(struct bsp_em7028_driver *const self);

    /* Read ID register (0x00); caller checks against EM7028_ID.        */
    em7028_status_t (*pf_read_id     )(struct bsp_em7028_driver *const self,
                                       uint8_t                  *const   id);

    /* Apply a full config struct in one shot; cached inside priv.      */
    em7028_status_t (*pf_apply_config)(struct bsp_em7028_driver *const self,
                                       em7028_config_t    const *const  cfg);

    /* Start sampling -- toggles HRS_CFG enable bits per cached cfg.    */
    em7028_status_t (*pf_start       )(struct bsp_em7028_driver *const self);

    /* Stop sampling -- clears HRS_CFG enable bits, keeps cfg cached.   */
    em7028_status_t (*pf_stop        )(struct bsp_em7028_driver *const self);

    /* Burst-read enabled HRS data registers and stamp the frame.       */
    em7028_status_t (*pf_read_frame  )(struct bsp_em7028_driver *const self,
                                       em7028_ppg_frame_t       *const frame);

    /* Low-level register access -- thresholds / INT_CTRL / debug.      */
    em7028_status_t (*pf_read_reg    )(struct bsp_em7028_driver *const self,
                                       uint8_t                     reg_addr,
                                       uint8_t                  *const   val);
    em7028_status_t (*pf_write_reg   )(struct bsp_em7028_driver *const self,
                                       uint8_t                     reg_addr,
                                       uint8_t                          val);
} bsp_em7028_driver_t;
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
* @brief Bind the injected interfaces and the internal vtable into a
*        caller-allocated driver instance, and reserve one opaque
*        private-state slot from a static pool. The EM7028 chip is NOT
*        accessed here -- the caller must invoke self->pf_init() via
*        the bound vtable before any I/O.
*
* @param[in]  p_iic       I2C transport interface (non-NULL).
* @param[in]  p_timebase  Monotonic ms tick interface (non-NULL).
* @param[in]  p_delay     DWT hardware delay interface (non-NULL).
* @param[in]  p_os_delay  RTOS yield delay interface (non-NULL).
*
* @param[out] self        Caller-allocated driver instance to populate.
*
* @return EM7028_OK             on success;
*         EM7028_ERRORPARAMETER for any NULL input;
*         EM7028_ERRORNOMEMORY  when the private-state pool is exhausted.
* */
em7028_status_t bsp_em7028_driver_inst(
                bsp_em7028_driver_t                 *const         self,
                em7028_iic_driver_interface_t const *const        p_iic,
                em7028_timebase_interface_t   const *const   p_timebase,
                em7028_delay_interface_t      const *const      p_delay,
                em7028_os_delay_interface_t   const *const   p_os_delay);
//******************************* Functions *********************************//

#endif /* __BSP_EM7028_DRIVER_H__ */
