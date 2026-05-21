/******************************************************************************
 * @file bsp_cst816t_driver.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief CST816T capacitive touch controller driver interface definitions.
 *
 * Defines the I2C, timebase, delay, and OS interface structures that must be
 * provided by the integration layer, together with the driver instance struct
 * and public API surface.
 *
 * Processing flow:
 *   1. Integration layer populates cst816t_driver_input_arg_t.
 *   2. bsp_cst816t_inst() binds interfaces, verifies chip ID, configures.
 *   3. Application polls finger/gesture/coordinate via vtable function ptrs.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_CST816T_DRIVER_H__
#define __BSP_CST816T_DRIVER_H__

//******************************** Includes *********************************//
#include <stdint.h>

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    CST816T_OK                      = 0,    /* Operation successful          */
    CST816T_ERROR                   = 1,    /* General error                 */
    CST816T_ERRORTIMEOUT            = 2,    /* Timeout error                 */
    CST816T_ERRORRESOURCE           = 3,    /* Resource unavailable          */
    CST816T_ERRORPARAMETER          = 4,    /* Invalid parameter             */
    CST816T_ERRORNOMEMORY           = 5,    /* Out of memory                 */
    CST816T_ERRORUNSUPPORTED        = 6,    /* Unsupported feature           */
    CST816T_ERRORISR                = 7,    /* ISR context error             */
    CST816T_RESERVED                = 0xFF, /* CST816T Reserved              */
} cst816t_status_t;

/*********************** Core Layer **********************/
/*      From HAL Layer: IIC Port     */
typedef struct 
{
    /*       hi2c pointer to a i2c handle structure      */
    void                     *                        hi2c;

    /*         i2c init and deinit interfaces            */
    cst816t_status_t (*pf_iic_init        )               \
                                      (void const * const);
    cst816t_status_t (*pf_iic_deinit      )               \
                                      (void const * const);

    /**/
    cst816t_status_t (*pf_iic_mem_write   ) (void    *i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t   *p_data,
                                        uint16_t     size,
                                        uint32_t  timeout);
    cst816t_status_t (*pf_iic_mem_read    ) (void    *i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t   *p_data,
                                        uint16_t     size,
                                        uint32_t  timeout);
} cst816t_iic_driver_interface_t;

/*        Delay Interface            */
typedef struct
{
    void             (*pf_delay_init       )        (void);
    void             (*pf_delay_ms         ) (uint32_t \
                                                 const ms);
    void             (*pf_delay_us         ) (uint32_t \
                                                 const us);
} cst816t_delay_interface_t;

/*        Yield Interface            */
typedef struct
{
    void             (*pf_rtos_yield       ) (uint32_t \
                                                 const ms);
} cst816t_os_delay_interface_t;

/*        Timebase Interface         */
typedef struct
{
    uint32_t         (*pf_get_tick_count   )        (void);
} cst816t_timebase_interface_t;

typedef struct
{
	uint16_t x_pos;
	uint16_t y_pos;
} cst816t_xy_t;

typedef enum
{
	NOGESTURE          = 	      0x00,
    UPGLIDE            = 	      0x01,
	DOWNGLIDE          = 	      0x02,
	RIGHTGLIDE         = 	      0x03,
	LEFTGLIDE          = 	      0x04,
	CLICK              = 	      0x05,
	DOUBLECLICK        =          0x0B,
	LONGPRESS          = 	      0x0C,
} cst816t_gesture_id_t;

typedef enum
{
	ERR_RESET_DIG      =          0x00,
	ERR_RESET_DOUBLE   =          0x01,
} cst816_err_reset_ctl_t;

typedef enum
{
	MOTION_DISABLE     =          0x00, 
	EN_CON_LR          =          0x01,
	EN_CON_UD          =          0x02, 
	EN_DCLICK          =          0x04,
	MOTION_ALLENABLE   =          0x05,
} cst816_motion_mask_t;

typedef enum
{
    /* IrqCtl @0xFA bit[0]; OR-combined with the bits below.  When set, a
     * long-press gesture emits a single low pulse instead of a periodic
     * pulse train.  0x00 was a header bug — useless as a bitmask. */
    ONCE_WLP           =          0x01,
    EN_MOTION          =          0x10, 
    EN_CHANGE          =          0x20, 
    EN_TOUCH           =          0x40,
    EN_TEST            =          0x80, 
} cst816_irq_ctl_t;


typedef struct bsp_cst816t_driver bsp_cst816t_driver_t;
struct bsp_cst816t_driver
{
    /*          core layer          */
    cst816t_iic_driver_interface_t      const  *         p_iic_driver_instance;
    cst816t_timebase_interface_t        const  *           p_timebase_instance;
    cst816t_delay_interface_t           const  *              p_delay_instance;
    cst816t_os_delay_interface_t        const  *                 p_os_instance;

    /******** CST816T initialization functions ***********/
    cst816t_status_t (*pf_cst816t_init  ) (struct bsp_cst816t_driver    const);
    cst816t_status_t (*pf_cst816t_deinit) (struct bsp_cst816t_driver *  const);


    /****** CST816T touch screen operation functions *****/
    cst816t_status_t (*pf_cst816t_get_gesture_id) (
                                    bsp_cst816t_driver_t   const, 
                                    cst816t_gesture_id_t * const p_gesture_id);
    cst816t_status_t (*pf_cst816t_get_xy_axis   ) (
                                    bsp_cst816t_driver_t   const, 
                                            cst816t_xy_t * const    p_xy_axis);
    cst816t_status_t (*pf_cst816t_get_chip_id   ) (
                                    bsp_cst816t_driver_t   const, 
                                                 uint8_t * const    p_chip_id);
    cst816t_status_t (*pf_cst816t_get_finger_num) (
                                    bsp_cst816t_driver_t   const, 
                                                 uint8_t * const p_finger_num);

    /********* CST816T configuration functions ***********/
    cst816t_status_t (*pf_cst816t_sleep              ) (
                                                   bsp_cst816t_driver_t const);
    cst816t_status_t (*pf_cst816t_wakeup             ) (
                                                   bsp_cst816t_driver_t const);
    cst816t_status_t (*pf_cst816t_set_err_reset_ctl  ) (
                                         bsp_cst816t_driver_t const, 
                                       cst816_err_reset_ctl_t   err_reset_ctl);
    cst816t_status_t (*pf_cst816t_set_long_press_tick) (
                                         bsp_cst816t_driver_t const,
                                                      uint8_t long_press_tick);
    cst816t_status_t (*pf_cst816t_set_motion_mask    ) (
                                         bsp_cst816t_driver_t const, 
                                         cst816_motion_mask_t     motion_mask);
    cst816t_status_t (*pf_cst816t_set_irq_pulse_width) (
                                         bsp_cst816t_driver_t const, 
                                                      uint8_t irq_pulse_width);
    // Set the normal scan period
    cst816t_status_t (*pf_cst816t_set_nor_scan_per            )(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t        scan_period);
    // Set the motion slope angle
    cst816t_status_t (*pf_cst816t_set_motion_slope_angle      )(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t x_right_y_up_angle);
    // Set the auto calibration period in low power mode
    cst816t_status_t (*pf_cst816t_set_low_power_auto_wake_time)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t               time);
    // Set the low power scan threshold
    cst816t_status_t (*pf_cst816t_set_lp_scan_th)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t          threshold);
    // Set the low power scan range, can select 0,1,2,3
    // 3 is the default range.
    cst816t_status_t (*pf_cst816t_set_lp_scan_win)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t             window);
    // Set the long press scan frequency, can select 1 - 255
    // 7 is the default frequency.
    cst816t_status_t (*pf_cst816t_set_lp_scan_freq)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t          frequency);
    // Set the low power scan current, can select 1 - 255
    cst816t_status_t (*pf_cst816t_set_lp_scan_idac)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t               idac);
    // Set the auto sleep time
    cst816t_status_t (*pf_cst816t_set_auto_sleep_time)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t               time);
    // Set the interrupt control
    cst816t_status_t (*pf_cst816t_set_irq_ctl)(
                                      bsp_cst816t_driver_t * const,
                                          cst816_irq_ctl_t               mode);
    // Set the auto reset time
    cst816t_status_t (*pf_cst816t_set_auto_reset)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t               time);
    // Set the long press time for reset
    cst816t_status_t (*pf_cst816t_set_long_press_time)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t               time);
    // Set the IO control mode
    cst816t_status_t (*pf_cst816t_set_io_ctl)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t               mode);
    // Disable auto sleep function
    cst816t_status_t (*pf_cst816t_disable_auto_sleep)(
                                      bsp_cst816t_driver_t * const,
                                                   uint8_t            disable);                                          
};

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
cst816t_status_t bsp_cst816t_inst(
    bsp_cst816t_driver_t                      * const               p_instance,
    cst816t_iic_driver_interface_t      const * const    p_iic_driver_instance,
    cst816t_timebase_interface_t        const * const      p_timebase_instance,
    cst816t_delay_interface_t           const * const         p_delay_instance,
    cst816t_os_delay_interface_t        const * const            p_os_instance,
    void (**pp_int_callback)(void *, void*)
);
//******************************* Functions *********************************//

#endif /* __BSP_CST816T_DRIVER_H__ */
