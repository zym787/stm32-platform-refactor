/******************************************************************************
 * @file bsp_wt588_driver.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Implement WT588 play/stop/volume operations and driver instantiation.
 *
 * Processing flow:
 * Validate interfaces, mount callbacks, initialize hardware, and expose APIs.
 * @version V1.0 2026-4-6
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wt588_driver.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Start playing a specific voice address
 *
 * Validates voice address range and sends the address code via PWM DMA interface.
 *
 * @param[in] self  : Pointer to WT588 driver instance
 * @param[in] index : Voice address to play (0-255)
 *
 * @return wt588_status_t WT588_OK if successful, error code otherwise
 *
 * */
static wt588_status_t (wt588_start_play)(bsp_wt588_driver_t *self, uint8_t index)
{
    wt588_status_t ret = WT588_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == self)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                 "wt588_start_play input error parameter");
        ret = WT588_ERRORPARAMETER;
        return ret;
    }
    if (index > WT588_MAX_PLAY_CODE)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                 "wt588_start_play input play code error");
        ret = WT588_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == self->p_pwm_dma_interface                ||
        NULL == self->p_pwm_dma_interface->\
                                     pf_pwm_dma_send_byte)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
               "wt588_start_play pwm dma interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    /******************* 3.Send voice address ************/
    ret = self->p_pwm_dma_interface->\
                               pf_pwm_dma_send_byte(index);
    if (WT588_OK != ret)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
                      "wt588_start_play send byte failed");
        return ret;
    }

    DEBUG_OUT(d, WT588_LOG_TAG,
              "wt588_start_play success, code=%u", (unsigned int)index);

    return ret;
}

/**
 * @brief Stop current playback
 *
 * Sends stop play command code via PWM DMA interface.
 *
 * @param[in] self : Pointer to WT588 driver instance
 *
 * @return wt588_status_t WT588_OK if successful, error code otherwise
 *
 * */
static wt588_status_t (wt588_stop_play )(bsp_wt588_driver_t *self)
{
    wt588_status_t ret = WT588_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == self)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
                 "wt588_stop_play input error parameter");
        ret = WT588_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == self->p_pwm_dma_interface                ||
        NULL == self->p_pwm_dma_interface->\
                                     pf_pwm_dma_send_byte)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
               "wt588_stop_play pwm dma interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    /******************* 3.Start play ********************/
    ret = self->p_pwm_dma_interface->\
                pf_pwm_dma_send_byte(WT588_STOP_PLAY_CODE);
    if (WT588_OK != ret)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
                      "wt588_stop_play send byte failed");
        return ret;
    }

    DEBUG_OUT(d, WT588_LOG_TAG, "wt588_stop_play success");

    return ret;
}

/**
 * @brief Set playback volume level
 *
 * Clamps volume to valid range and sends volume command via PWM DMA interface.
 *
 * @param[in] self   : Pointer to WT588 driver instance
 * @param[in] volume : Volume level (0-31)
 *
 * @return wt588_status_t WT588_OK if successful, error code otherwise
 *
 * */
static wt588_status_t (wt588_set_volume)(bsp_wt588_driver_t *self, uint8_t volume)
{
    wt588_status_t ret = WT588_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == self)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
                 "wt588_set_volume input error parameter");
        ret = WT588_ERRORPARAMETER;
        return ret;
    }
    if (volume > WT588_MAX_VOLUME_CODE)
    {
        DEBUG_OUT(w, WT588_LOG_TAG, 
                           "Volume exceeds maximum value");
        volume = WT588_MAX_VOLUME_CODE;
    }
    if (volume < WT588_MIN_VOLUME_CODE)
    {
        DEBUG_OUT(w, WT588_LOG_TAG,
                          "Volume is below minimum value");
        volume = WT588_MIN_VOLUME_CODE;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == self->p_pwm_dma_interface                ||
        NULL == self->p_pwm_dma_interface->\
                                     pf_pwm_dma_send_byte)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
               "wt588_set_volume pwm dma interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    /******************* 3.Start play ********************/
    ret = self->p_pwm_dma_interface->\
                              pf_pwm_dma_send_byte(volume);
    if (WT588_OK != ret)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG,
                      "wt588_set_volume send byte failed");
        return ret;
    }

    DEBUG_OUT(d, WT588_LOG_TAG,
              "wt588_set_volume success, volume=%x", (unsigned int)volume);

    return ret;
}

/**
 * @brief Check if WT588 module is busy playing
 *
 * Delegates to busy interface callback to read hardware busy signal.
 *
 * @param[in] self : Pointer to WT588 driver instance
 *
 * @return bool true if busy, false otherwise (or on error)
 *
 * */
static bool           (wt588_is_busy   )(bsp_wt588_driver_t *self)
{
    /************ 1.Checking input parameters ************/
    if (NULL == self)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                    "wt588_is_busy input error parameter");
        return false;
    }
    /************* 2.Checking the Resources **************/
    if (NULL == self->p_busy_interface                   ||
        NULL == self->p_busy_interface->pf_is_busy)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                     "wt588_is_busy busy interface error");
        return false;
    }

    return self->p_busy_interface->pf_is_busy();
}

/**
 * @brief Initialize WT588 hardware interfaces
 *
 * Initializes GPIO and PWM DMA interfaces with proper error cleanup.
 *
 * @param[in] self : Pointer to WT588 driver instance
 *
 * @return wt588_status_t WT588_OK if successful, error code otherwise
 *
 * */
static wt588_status_t wt588_driver_init(bsp_wt588_driver_t const * const self)
{
    wt588_status_t ret = WT588_OK;
    /************ 1.Checking input parameters ************/
    /************* 2.Checking the Resources **************/
    // has been checked in wt588_driver_inst

    ret = self->p_gpio_interface->pf_wt_gpio_init();
    if (WT588_OK != ret)
    {
        self->p_gpio_interface->pf_wt_gpio_deinit();

        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                  "wt588_driver_init gpio interface init failed");
        return ret;
    }
    DEBUG_OUT(d, WT588_LOG_TAG, "wt588 gpio interface init successful");

    ret = self->p_pwm_dma_interface->pf_pwm_dma_init();
    if (WT588_OK != ret)
    {
        self->p_gpio_interface->pf_wt_gpio_deinit();
        self->p_pwm_dma_interface->pf_pwm_dma_deinit();

        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
               "wt588_init pwm dma interface init failed");
        return ret;
    }

    DEBUG_OUT(d, WT588_LOG_TAG, "wt588_driver_init successful");

    return ret;
}

/**
 * @brief Initialize WT588 driver instance
 *
 * Validates interfaces, mounts callbacks, and initializes hardware.
 * Follows structured error checking pattern.
 *
 * @param[in] p_wt588_inst        : Pointer to WT588 driver instance
 * @param[in] p_sys_interface     : Pointer to system delay interface
 * @param[in] p_busy_interface    : Pointer to busy detection interface
 * @param[in] p_gpio_interface    : Pointer to GPIO control interface
 * @param[in] p_pwm_dma_interface : Pointer to PWM DMA interface
 *
 * @return wt588_status_t WT588_OK if successful, error code otherwise
 *
 * */
wt588_status_t wt588_driver_inst(
                          bsp_wt588_driver_t       * const        p_wt588_inst,
                          wt_sys_interface_t       * const     p_sys_interface,
                         wt_busy_interface_t       * const    p_busy_interface,
                         wt_gpio_interface_t       * const    p_gpio_interface,
                      wt_pwm_dma_interface_t       * const p_pwm_dma_interface)
{
    DEBUG_OUT(i, WT588_LOG_TAG, "wt588_driver_inst start");
    wt588_status_t ret = WT588_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == p_wt588_inst                             || 
        NULL == p_sys_interface                          || 
        NULL == p_busy_interface                         ||
        NULL == p_gpio_interface                         ||
        NULL == p_pwm_dma_interface)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                       "wt588_inst input error parameter");
        ret = WT588_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == p_sys_interface->pf_delay_ms)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                              "wt588 sys interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    if (NULL == p_busy_interface->pf_is_busy)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                             "wt588 busy interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    if (NULL == p_gpio_interface->pf_wt_gpio_init        ||
        NULL == p_gpio_interface->pf_wt_gpio_deinit)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                             "wt588 gpio interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    if (NULL == p_pwm_dma_interface->pf_pwm_dma_init     ||
        NULL == p_pwm_dma_interface->pf_pwm_dma_deinit   ||
        NULL == p_pwm_dma_interface->pf_pwm_dma_send_byte)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                          "wt588 pwm dma interface error");
        ret = WT588_ERRORRESOURCE;
        return ret;
    }

    /**************** 3.Mount interfaces *****************/
    // 3.1 mount external interfaces
    p_wt588_inst->p_busy_interface    =   p_busy_interface;
    p_wt588_inst->p_gpio_interface    =   p_gpio_interface;
    p_wt588_inst->p_sys_interface     =    p_sys_interface;
    p_wt588_inst->p_pwm_dma_interface =p_pwm_dma_interface;

    // 3.2 mount internal interfaces
    p_wt588_inst->pf_start_play       =   wt588_start_play;
    p_wt588_inst->pf_stop_play        =    wt588_stop_play;
    p_wt588_inst->pf_set_volume       =   wt588_set_volume;
    p_wt588_inst->pf_is_busy          =      wt588_is_busy;

    ret = wt588_driver_init(p_wt588_inst);
    if (WT588_OK != ret)
    {
        DEBUG_OUT(e, WT588_ERR_LOG_TAG, 
                             "wt588 instance init failed");
        return ret;
    }

    DEBUG_OUT(d, WT588_LOG_TAG, "wt588_driver_inst successful");

    return ret;
}            

//******************************* Functions *********************************//
