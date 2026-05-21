/******************************************************************************
 * @file       bsp_mpuxxxx_driver.c
 *
 * @par        dependencies
 * - bsp_mpuxxxx_driver.h
 *
 * @author     Ethan-Hang
 *
 * @brief      MPUxxxx (e.g. MPU6050) driver implementation.
 *
 * Processing flow:
 * - Bind external interfaces via instantiation helper.
 * - Initialize bus, reset device, configure defaults, verify ID.
 * - Provide register helpers for accel/gyro/temp access.
 * @version    V1.0 2026-1-6
 *
 * @note       1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_mpuxxxx_driver.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define MPUXXXX_IS_INITED                              true
#define MPUXXXX_NOT_INITED                            false
#define TIME_OUT_MS                                    1000
#define IIC_MEM_SIZE_8BIT                       0x00000001U
#define MPU6050_DATA_PACKET_SIZE                         14

static bool g_mpuxxx_is_inited     =     MPUXXXX_NOT_INITED;

/*
// #define MPUXXXX_WRITE_REG(p_mpu_dirver, reg, p_data, len) \
//     p_mpu_dirver->p_iic_driver_instance->pf_iic_mem_write(\
//         p_mpu_dirver->p_iic_driver_instance->hi2c,        \
//         (MPU_ADDR << 1) | 0,                              \
//         reg,                                              \
//         IIC_MEM_SIZE_8BIT,                                \
//         p_data,                                           \
//         len,                                              \
//         TIME_OUT_MS                                       \
//     )


// #define MPUXXXX_READ_REG(p_mpu_dirver, reg, p_data, len)  \
//     p_mpu_dirver->p_iic_driver_instance->pf_iic_mem_read( \
//         p_mpu_dirver->p_iic_driver_instance->hi2c,        \
//         (MPU_ADDR << 1) | 1,                              \
//         reg,                                              \
//         IIC_MEM_SIZE_8BIT,                                \
//         p_data,                                           \
//         len,                                              \
//         TIME_OUT_MS                                       \
//     )
*/

/*  =======================================================================  */

/*   ==================== Inline Function Replacements ===================   */
/**
 * @brief Write register via I2C interface
 * @param p_mpu_driver: Pointer to MPU driver instance
 * @param reg: Register address
 * @param p_data: Pointer to data buffer
 * @param len: Number of bytes to write
 * @return Status code from I2C write operation
 */
static inline mpuxxxx_status_t mpuxxxx_write_reg(
    struct bsp_mpuxxxx_driver const *p_mpu_driver,
    uint16_t reg,
    uint8_t *p_data,
    uint16_t len)
{
    return p_mpu_driver->p_iic_driver_instance->pf_iic_mem_write(
        p_mpu_driver->p_iic_driver_instance->hi2c,
        (MPU_ADDR << 1) | 0,
        reg,
        IIC_MEM_SIZE_8BIT,
        p_data,
        len,
        TIME_OUT_MS);
}

/**
 * @brief Read register via I2C interface
 * @param p_mpu_driver: Pointer to MPU driver instance
 * @param reg: Register address
 * @param p_data: Pointer to data buffer
 * @param len: Number of bytes to read
 * @return Status code from I2C read operation
 */
static inline mpuxxxx_status_t mpuxxxx_read_reg(
    struct bsp_mpuxxxx_driver const *p_mpu_driver,
    uint16_t reg,
    uint8_t *p_data,
    uint16_t len)
{
    return p_mpu_driver->p_iic_driver_instance->pf_iic_mem_read(
        p_mpu_driver->p_iic_driver_instance->hi2c,
        (MPU_ADDR << 1) | 1,
        reg,
        IIC_MEM_SIZE_8BIT,
        p_data,
        len,
        TIME_OUT_MS);
}
/* ====================================================================== */

/**
 * @brief      Gyroscope scale factor (LSB to dps) based on FSR.
 *
 * @param[in]  : None.
 *
 * @param[out] : None.
 *
 * @return     : None.
 * */
static double gs_gyro_scale        =                131.0f;

/**
 * @brief      Accelerometer scale factor (LSB to g) based on FSR.
 *
 * @param[in]  : None.
 *
 * @param[out] : None.
 *
 * @return     : None.
 * */
static double gs_accel_scale       =              16384.0f;

//****************************** Defines *****************************//

//****************************** Declaring ****************************//
/**
 * @brief      Deinitialize MPUxxxx driver and release I2C.
 *
 * @param[in]  this : Pointer to driver instance to deinitialize.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success, error code otherwise.
 * */                                     
static mpuxxxx_status_t (mpuxxxx_deinit          )
                                      (bsp_mpuxxxx_driver_t const * const this)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized 1!");
        return ret;
    }

    this->p_iic_driver_instance->pf_iic_deinit\
                       (this->p_iic_driver_instance->hi2c);

    return ret;
}

/**
 * @brief      Put the device into sleep mode.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_sleep           )
                                      (bsp_mpuxxxx_driver_t const * const this)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized 2!");
        return ret;
    }

    uint8_t data = 0;
    data = SLEEP_BIT(1);

    ret = mpuxxxx_write_reg(this, MPU_PWR_MGMT1_REG, &data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx write register failed!");
    }
    return ret;
}


/**
 * @brief      Wake the device from sleep.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_wakeup          )
                                      (bsp_mpuxxxx_driver_t const * const this)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 2");
        return ret;
    }

    uint8_t data = 0;
    data = SLEEP_BIT(0);

    ret = mpuxxxx_write_reg(this, MPU_PWR_MGMT1_REG, &data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_PWR_MGMT1_REG register failed!");
    }
    return ret;
}


/**
 * @brief      Set gyro full-scale range and update scale factor.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  fsr  : Full-scale range value to write.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_gyro_fsr    )
                                      (bsp_mpuxxxx_driver_t const * const this,
                                                                   uint8_t fsr)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 4");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_GYRO_CFG_REG, &fsr, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_GYRO_CFG_REG register failed!");
    }

    switch (fsr >> (2) & 0x03)
    {
    case 0x00:
        gs_gyro_scale = 131.0f;
        break;
    case 0x01:
        gs_gyro_scale =  65.5f;
        break;
    case 0x02:
        gs_gyro_scale =  32.8f;
        break;
    case 0x03:
        gs_gyro_scale =  16.4f;
        break;
    default:
        gs_gyro_scale = 131.0f;
        break;
    }

    return ret;
}

/**
 * @brief      Set accel full-scale range and update scale factor.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  fsr  : Full-scale range value to write.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_accel_fsr   )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                   uint8_t fsr)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 3");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_ACCEL_CFG_REG, &fsr, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_ACCEL_CFG_REG register failed!");
    }

    switch (fsr >> (2) & 0x03)
    {
    case 0x00:
        gs_accel_scale = 16384.0f;
        break;
    case 0x01:
        gs_accel_scale =  8192.0f;
        break;
    case 0x02:
        gs_accel_scale =  4096.0f;
        break;
    case 0x03:
        gs_accel_scale =  2048.0f;
        break;
    default:
        gs_accel_scale = 16384.0f;
        break;
    }
    return ret;
}

/**
 * @brief      Configure the digital low-pass filter.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  lpf  : DLPF configuration value.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_lpf         )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                   uint8_t lpf)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 6");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_CFG_REG, &lpf, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_CFG_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the sample rate divider.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  rate : Sample rate divider value.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_rate        )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                  uint8_t rate)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 5");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_SAMPLE_RATE_REG, &rate, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_SAMPLE_RATE_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Enable or disable device interrupts.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[in]  enable : Interrupt enable mask to write.
 *
 * @param[out]        : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_interrupt_enable    )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                uint8_t enable)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx fifo already initialized! 4");
        return ret;
    }


    ret = mpuxxxx_write_reg(this, MPU_INT_EN_REG, &enable, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_INT_EN_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Set motion detection threshold.
 *
 * @param[in]  this       : Driver instance.
 *
 * @param[in]  threshold  : Motion threshold value to write.
 *
 * @param[out]            : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_motion_threshold    )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                             uint8_t threshold)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized 9!");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_MOTION_DET_REG, &threshold, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_MOTION_DET_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Configure interrupt pin level and polarity.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[in]  level  : Interrupt level configuration value.
 *
 * @param[out]        : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_int_level   )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                 uint8_t level)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx fifo already initialized! 3");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_INTBP_CFG_REG, &level, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_INTBP_CFG_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Write USER_CTRL register settings.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  data : USER_CTRL register value to write.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_user_ctrl   )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                  uint8_t data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx fifo already initialized! 1");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_USER_CTRL_REG, &data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_USER_CTRL_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Write power management register 1.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  data : Register value (clock, sleep, reset bits).
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_pwr_mgmt1_reg       )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                  uint8_t data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 8");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_PWR_MGMT1_REG, &data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_PWR_MGMT1_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Write power management register 2.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  data : Standby/enable mask for gyro and accel axes.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_pwr_mgmt2_reg       )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                  uint8_t data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized! 9");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_PWR_MGMT2_REG, &data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_PWR_MGMT2_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Enable or disable FIFO sources.
 *
 * @param[in]  this : Driver instance.
 *
 * @param[in]  data : FIFO enable mask to write.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_set_fifo_en_reg         )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                                                  uint8_t data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx fifo already initialized! 2");
        return ret;
    }

    ret = mpuxxxx_write_reg(this, MPU_FIFO_EN_REG, &data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx write MPU_FIFO_EN_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Read temperature and convert to Celsius.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Output data structure.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_get_temperature         )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                             mpuxxxx_data_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized! 13");
        return ret;
    }

    uint8_t temp_buf[2] = {0};
    ret = mpuxxxx_read_reg(this, MPU_TEMP_OUTH_REG, temp_buf, 2);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read MPU_TEMP_OUTH_REG register failed!");
        return ret;
    }

    int16_t temp_raw      = (int16_t)((*(temp_buf)) << 8 | (*(temp_buf + 1)));
    p_data->temperature_c = ((float)temp_raw / 340.0f + 36.53f);

    return ret;
}

/**
 * @brief      Read accelerometer data and convert to g.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Output data structure.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_get_accel       )
                                      (bsp_mpuxxxx_driver_t const * const this,
                                             mpuxxxx_data_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized! 14");
        return ret;
    }

    uint8_t accel_buf[6] = {0};
    ret = mpuxxxx_read_reg(this, MPU_ACCEL_XOUTH_REG, accel_buf, 6);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read MPU_ACCEL_XOUTH_REG register failed!");
        return ret;
    }

    p_data->accel_x_raw = (int16_t)((*(accel_buf    )) << (8) | 
                                    (*(accel_buf + 1)));
    p_data->accel_y_raw = (int16_t)((*(accel_buf + 2)) << (8) |
                                    (*(accel_buf + 3)));
    p_data->accel_z_raw = (int16_t)((*(accel_buf + 4)) << (8) | 
                                    (*(accel_buf + 5)));

    p_data->accel_x_g = ((double)p_data->accel_x_raw / gs_accel_scale);
    p_data->accel_y_g = ((double)p_data->accel_y_raw / gs_accel_scale);
    p_data->accel_z_g = ((double)p_data->accel_z_raw / gs_accel_scale);

    return ret;
}

/**
 * @brief      Read gyroscope data and convert to dps.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Output data structure.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_get_gyro        )
                                      (bsp_mpuxxxx_driver_t const * const this, 
                                             mpuxxxx_data_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized! 15");
        return ret;
    }

    uint8_t gyro_buf[6] = {0};
    ret = mpuxxxx_read_reg(this, MPU_GYRO_XOUTH_REG, gyro_buf, 6);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read MPU_GYRO_XOUTH_REG register failed!");
        return ret;
    }

    p_data->gyro_x_raw = (int16_t)((*(gyro_buf    )) << (8) | 
                                   (*(gyro_buf + 1)));
    p_data->gyro_y_raw = (int16_t)((*(gyro_buf + 2)) << (8) | 
                                   (*(gyro_buf + 3)));
    p_data->gyro_z_raw = (int16_t)((*(gyro_buf + 4)) << (8) | 
                                   (*(gyro_buf + 5)));
    
    p_data->gyro_x_dps = ((double)p_data->gyro_x_raw / gs_gyro_scale);
    p_data->gyro_y_dps = ((double)p_data->gyro_y_raw / gs_gyro_scale);
    p_data->gyro_z_dps = ((double)p_data->gyro_z_raw / gs_gyro_scale);

    return ret;
}

/**
 * @brief      Read accel, temp, and gyro data in one burst.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Output data structure.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_get_all_data    )
                                    (bsp_mpuxxxx_driver_t const * const   this, 
                                             mpuxxxx_data_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized! 16");
        return ret;
    }

    uint8_t data_buf[14] = {0};
    ret = mpuxxxx_read_reg(this, MPU_ACCEL_XOUTH_REG, data_buf, 14);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read MPU_ACCEL_XOUTH_REG register failed!");
        return ret;
    }
    p_data->accel_x_raw   = (int16_t)((*(data_buf     )) << (8) | 
                                      (*(data_buf +  1)));
    p_data->accel_y_raw   = (int16_t)((*(data_buf +  2)) << (8) |
                                      (*(data_buf +  3)));
    p_data->accel_z_raw   = (int16_t)((*(data_buf +  4)) << (8) | 
                                      (*(data_buf +  5)));
    p_data->accel_x_g     = ((double)p_data->accel_x_raw / gs_accel_scale);
    p_data->accel_y_g     = ((double)p_data->accel_y_raw / gs_accel_scale);
    p_data->accel_z_g     = ((double)p_data->accel_z_raw / gs_accel_scale);

    int16_t temp_raw      = (int16_t)((*(data_buf + 6)) << 8 | (*(data_buf + 7)));
    p_data->temperature_c = ((float)temp_raw / 340.0f + 36.53f);

    p_data->gyro_x_raw    = (int16_t)((*(data_buf +  8)) << (8) | 
                                      (*(data_buf +  9)));
    p_data->gyro_y_raw    = (int16_t)((*(data_buf + 10)) << (8) | 
                                      (*(data_buf + 11)));
    p_data->gyro_z_raw    = (int16_t)((*(data_buf + 12)) << (8) | 
                                      (*(data_buf + 13)));
    p_data->gyro_x_dps    = ((double)p_data->gyro_x_raw / gs_gyro_scale);
    p_data->gyro_y_dps    = ((double)p_data->gyro_y_raw / gs_gyro_scale);
    p_data->gyro_z_dps    = ((double)p_data->gyro_z_raw / gs_gyro_scale);

    return ret;
}

/**
 * @brief      Read interrupt status register.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Interrupt status value.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_get_interrupt_status_reg)
                                    (bsp_mpuxxxx_driver_t const * const   this,
                                                    uint8_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized! 17");
        return ret;
    }
    
    ret = mpuxxxx_read_reg(this, MPU_INT_STA_REG, p_data, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read MPU_INT_STA_REG register failed!");
        return ret;
    }
    DEBUG_OUT(i, MPUXXXX_LOG_TAG, "mpuxxxx interrupt status register: 0x%02X",
              *p_data);
    return ret;
}

/**
 * @brief      Read FIFO packets and convert accel/gyro data.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Output buffer for parsed FIFO packets.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_read_fifo_packet)
                                    (bsp_mpuxxxx_driver_t const * const   this, 
                                             mpuxxxx_data_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized!");
        return ret;
    }
    uint8_t fifo_buffer[12] = {0};
    mpuxxxx_read_reg(this, MPU_FIFO_CNTH_REG, fifo_buffer, 2);
    uint16_t fifo_len = ((*(fifo_buffer)) << (8) | (*(fifo_buffer + 1)));

    uint16_t fifo_count = 0;
    if (fifo_len >= 12)
    {
        fifo_count = fifo_len / 12;
        for (uint16_t i = 0; i < fifo_count; i++)
        {
            memset(fifo_buffer, 0, sizeof(fifo_buffer));
            ret = mpuxxxx_read_reg(this, MPU_FIFO_RW_REG, fifo_buffer, 12);
            if (MPUXXXX_OK != ret)
            {
                DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                          "mpuxxxx read MPU_FIFO_RW_REG register failed!");
                return ret;
            }

            // Process FIFO data here
            (p_data + i)->accel_x_raw = (int16_t)((*(fifo_buffer     ))<<(8) | 
                                                  (*(fifo_buffer +  1)));
            (p_data + i)->accel_y_raw = (int16_t)((*(fifo_buffer +  2))<<(8) | 
                                                  (*(fifo_buffer +  3)));
            (p_data + i)->accel_z_raw = (int16_t)((*(fifo_buffer +  4))<<(8) | 
                                                  (*(fifo_buffer +  5)));
            (p_data + i)->gyro_x_raw  = (int16_t)((*(fifo_buffer +  6))<<(8) | 
                                                  (*(fifo_buffer +  7)));
            (p_data + i)->gyro_y_raw  = (int16_t)((*(fifo_buffer +  8))<<(8) | 
                                                  (*(fifo_buffer +  9)));
            (p_data + i)->gyro_z_raw  = (int16_t)((*(fifo_buffer + 10))<<(8) | 
                                                  (*(fifo_buffer + 11)));
            (p_data + i)->accel_x_g   = ((double)(p_data + i)->accel_x_raw  /\
                                                              gs_accel_scale);
            (p_data + i)->accel_y_g   = ((double)(p_data + i)->accel_y_raw  /\
                                                              gs_accel_scale);
            (p_data + i)->accel_z_g   = ((double)(p_data + i)->accel_z_raw  /\
                                                              gs_accel_scale);
            (p_data + i)->gyro_x_dps  = ((double)(p_data + i)->gyro_x_raw   /\
                                                               gs_gyro_scale);
            (p_data + i)->gyro_y_dps  = ((double)(p_data + i)->gyro_y_raw   /\
                                                               gs_gyro_scale);
            (p_data + i)->gyro_z_dps  = ((double)(p_data + i)->gyro_z_raw   /\
                                                               gs_gyro_scale); 
#if MPUXXXX_DEBUG
            DEBUG_OUT(i, "mpuxxxx fifo packet %d:"  
                         "ax_raw=%d, ay_raw=%d, az_raw=%d", 
                         "gx_raw=%d, gy_raw=%d, gz_raw=%d",
                      i,
                      (p_data + i)->accel_x_raw,
                      (p_data + i)->accel_y_raw,
                      (p_data + i)->accel_z_raw,
                      (p_data + i)->gyro_x_raw,
                      (p_data + i)->gyro_y_raw,
                      (p_data + i)->gyro_z_raw);
#endif // MPUXXXX_DEBUG
        }
    }

    return ret;
}

/**
 * @brief      Placeholder for FIFO read when ISR occurs.
 *
 * @param[in]  this   : Driver instance.
 *
 * @param[out] p_data : Output buffer for parsed FIFO packets.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_read_fifo_isr_occur)
                                    (bsp_mpuxxxx_driver_t const * const   this,
                                             mpuxxxx_data_t     * const p_data)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this || NULL == p_data)
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxs is initialized
    if (MPUXXXX_NOT_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx not initialized!");
        return ret;
    }

    return ret;
}

/**
 * @brief      Initialize MPUxxxx, reset, configure defaults, verify ID.
 *
 * @param[in]  this : Driver instance with mounted interfaces.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_init            ) 
                                      (bsp_mpuxxxx_driver_t const * const this)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;
    // 1. check input parameter
    if (NULL == this                                     ||
        NULL == this->p_iic_driver_instance->pf_iic_init
#if OS_SUPPORTING
     || NULL == this->p_yield_instance                   ||
        NULL == this->p_yield_instance->pf_rtos_yield    
#else
     || NULL == this->p_delay_instance                   ||
        NULL == this->p_delay_instance->pf_delay_ms      
#endif // OS_SUPPORTING
    )
    {
        ret = MPUXXXX_ERRORPARAMETER;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx input error parameter!");
        return ret;
    }

    // 2. check if mpuxxxx is initialized
    if (MPUXXXX_IS_INITED == this->private_data)
    {
        ret = MPUXXXX_ERRORRESOURCE;
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx already initialized 0!");
        return ret;
    }

    // 3. initialize iic bus
    ret = this->p_iic_driver_instance->pf_iic_init\
                       (this->p_iic_driver_instance->hi2c);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx iic init failed!");
        return ret;
    }

    // 4. reset device
    ret = mpuxxxx_set_pwr_mgmt1_reg(this, DEVICE_RESET_BIT(1));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx set pwr_mgmt1_reg failed!");
        return ret;
    }

    // 5. delay 100ms for device reset
#if OS_SUPPORTING
    this->p_yield_instance->pf_rtos_yield(100);
#else
    this->p_delay_instance->pf_delay_ms(100);
#endif // OS_SUPPORTING

    // 6. wakeup device
    ret = mpuxxxx_wakeup(this);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx wakeup failed!");
        return ret;
    }

    // 7. set default configurations
    // 7.1 set accel fsm
    ret = mpuxxxx_set_accel_fsr(this, AFS_SEL_BIT(0x01));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx set accel fsr failed!");
        return ret;
    }
    // 7.2 set gyro fsm
    ret = mpuxxxx_set_gyro_fsr(this, FS_SEL_BIT(0x03));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx set gyro fsr failed!");
        return ret;
    }
    // 7.3 set sample rate
    // sample rate = gyro output rate / (1 + SMPLRT_DIV)
    // gyro output rate = 1kHz when DLPF is enabled
    // sample rate = 1000 / (1 + 19) = 50Hz
    ret = mpuxxxx_set_rate(this, SMPLRT_DIV_BIT(0x19));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx set sample rate failed!");
        return ret;
    }
    // 7.4 set low pass filter
    // sample rate = 50Hz, set DLPF to 25Hz
    ret = mpuxxxx_set_lpf(this, DLPF_CFG_BIT(0x05));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx set low pass filter failed!");
        return ret;
    }
    // // 7.5 set interrupt enable
    // ret = mpuxxxx_set_interrupt_enable(this, DATA_RDY_EN_BIT(1));
    // if (MPUXXXX_OK != ret)
    // {
    //     DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
    //               "mpuxxxx set interrupt enable failed!");
    //     return ret;
    // }

    // 8. verify device ID
    uint8_t device_id = 0;
    ret = mpuxxxx_read_reg(this, MPU_DEVICE_ID_REG, &device_id, 1);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read device ID register failed!");
        return ret;
    }
    if (MPU_ID != device_id)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx device ID verify failed!");
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx read device ID: 0x%02X, "
                  "expected device ID: 0x%02X",
                  device_id, MPU_ID);
        ret = MPUXXXX_ERROR;
        return ret;
    }

    // 9. set pll with x-axis gyro as clock source reference
    ret = mpuxxxx_set_pwr_mgmt1_reg(this, CLKSEL_BIT(0x01));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx set pwr_mgmt1_reg failed!");
        return ret;
    }

    // 10. Disable gyroscope standby
    ret = mpuxxxx_set_pwr_mgmt2_reg(this,  STBY_XG_BIT(0) | 
                                           STBY_YG_BIT(0) | 
                                           STBY_ZG_BIT(0) |
                                           STBY_XA_BIT(0) |
                                           STBY_YA_BIT(0) |
                                           STBY_ZA_BIT(0));
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx set pwr_mgmt2_reg failed!");
        return ret;
    }

    return ret;
}

/**
 * @brief      Initialize FIFO: reset, enable, configure interrupt.
 *
 * @param[in]  this : Driver instance with initialized device.
 *
 * @param[out]      : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t (mpuxxxx_fifo_init       )
                                      (bsp_mpuxxxx_driver_t const * const this)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;

    // 1. reset fifo
    ret = mpuxxxx_set_user_ctrl(this, FIFO_RESET_BIT(1));
    if (ret != MPUXXXX_OK)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxx_fifo_init reset fifo error\r\n");
        return ret;
    }

    // 1.1 Wait for reset to complete
#ifdef OS_SUPPORTING
    this->p_yield_instance->pf_rtos_yield(10);
#else
    this->p_delay_instance->pf_delay_ms(10);
#endif

    // 2. Configure FIFO enable register, note the configuration order
    ret = mpuxxxx_set_fifo_en_reg(this,
                                 ACCEL_FIFO_EN_BIT(1) |
                                 XG_FIFO_EN_BIT(1)    |
                                 YG_FIFO_EN_BIT(1)    |
                                 ZG_FIFO_EN_BIT(1));
    if (ret != MPUXXXX_OK)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx_fifo_init configure fifo "
                  "enable register error\r\n");
        return ret;
    }

    // 3. Enable FIFO last
    ret = mpuxxxx_set_user_ctrl(this, FIFO_EN_BIT(1));
    if (ret != MPUXXXX_OK)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx_fifo_init enable fifo error\r\n");
        return ret;
    }

    // 4. Configure interrupt
    ret = mpuxxxx_set_int_level(this, INT_RD_CLEAR_BIT(1) | INT_LEVEL_BIT(1));
    if (ret != MPUXXXX_OK)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx_fifo_init configure interrupt error\r\n");
        return ret;
    }

    // 5. Enable FIFO overflow interrupt and DATA_RDY interrupt
    ret = mpuxxxx_set_interrupt_enable(this, FIFO_OVERFLOW_EN_BIT(1) |
                                             DATA_RDY_EN_BIT(1));
    if (ret != MPUXXXX_OK)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx_fifo_init enable fifo overflow "
                  "interrupt error\r\n");
        return ret;
    }

    return ret;
}

/**
 * @brief      Initialize MPU driver: init device, then init FIFO.
 *
 * @param[in]  p_mpuxxxx_driver : Driver instance with mounted interfaces.
 *
 * @param[out]                  : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
static mpuxxxx_status_t bsp_mpuxxxx_driver_init
                          (bsp_mpuxxxx_driver_t const * const p_mpuxxxx_driver)
{
    mpuxxxx_status_t ret = MPUXXXX_OK;

    // 1. check input parameter
    // has been checked in upper layer

    // 2. initialize mpuxxxx
    ret = mpuxxxx_init(p_mpuxxxx_driver);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx init failed!");
        return ret;
    }

    // 3. initialize fifo
    ret = mpuxxxx_fifo_init(p_mpuxxxx_driver);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG, "mpuxxxx fifo init failed!");
        return ret;
    }

    return ret;
}

void int_interrupt_callback(void const * const this, void * const p_data)
{
    // 1. check input parameter
    if (NULL == this)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx int interrupt callback input error parameter!");
        return;
    }

    // 2. get driver instance
    bsp_mpuxxxx_driver_t *p_driver = (bsp_mpuxxxx_driver_t *)this;

#if !(OS_SUPPORTING)
    // bare-metal path: synchronously read out all data here
    mpuxxxx_status_t ret = mpuxxxx_get_all_data(p_driver,
                                                (mpuxxxx_data_t *)p_data);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx get all data failed in interrupt callback!");
        return;
    }
#else
    // OS path: wake the handler thread via task notification so the bus mutex
    // can be held around the non-blocking DMA read. Doing the DMA from ISR
    // was a bug: if another task held the bus, HAL_I2C_Mem_Read_DMA returned
    // HAL_BUSY and the MPU thread never progressed. Disabling MPU INT_EN and
    // starting DMA now happen in task context (see mpuxxxx_handler_thread).
    (void)p_data;
    if (NULL == p_driver->p_os_instance                              ||
        NULL == p_driver->p_os_instance->pf_os_semaephore_notify_isr ||
        NULL == p_driver->notify_handler)
    {
        return;
    }

    long higher_priority_task_woken = 0;
    /* eAction = 1 : OSAL_NOTIFY_SET_BITS (mirrors FreeRTOS eSetBits).
       Any non-zero value wakes the handler; bits are cleared on wait exit. */
    p_driver->p_os_instance->pf_os_semaephore_notify_isr(
        (void *)p_driver->notify_handler,
        1U,
        1U,
        &higher_priority_task_woken);
#endif // !(OS_SUPPORTING)
}

void dma_interrupt_callback(void const * const this, void * const p_data)
{
    (void)this;
    (void)p_data;
    /* DMA completion is now waited inside the I2C abstraction.
       Keep callback hook for compatibility with HAL weak callback path. */
}

/**
 * @brief      Instantiate driver object and bind external interfaces.
 *
 * @param[in]  p_mpuxxxx_driver       : Driver object to populate.
 *
 * @param[in]  p_iic_driver_interface : I2C interface implementation.
 *
 * @param[in]  p_interrupt_interface  : Interrupt interface implementation.
 *
 * @param[in]  p_timebase_interface   : Timebase provider.
 *
 * @param[in]  p_delay_interface      : Delay provider (ms/us).
 *
 * @param[out]                        : None.
 *
 * @return     mpuxxxx_status_t MPUXXXX_OK on success; error code otherwise.
 * */
mpuxxxx_status_t bsp_mpuxxxx_driver_inst(
           bsp_mpuxxxx_driver_t                 * const       p_mpuxxxx_driver,

           iic_driver_interface_t         const * const p_iic_driver_interface,
           hardware_interrupt_interface_t const * const  p_interrupt_interface,
           timebase_interface_t           const * const   p_timebase_interface,
           delay_interface_t              const * const      p_delay_interface,

#if OS_SUPPORTING
           yield_interface_t              const * const      p_yield_interface,
           os_interface_t                 const * const         p_os_interface,

#endif // OS_SUPPORTING
           void (*callback_register    )      
                           (void (*callback)(void const * const, void* const)),
           void (*callback_register_dma)      
                           (void (*callback)(void const * const, void* const))
)
{
    DEBUG_OUT(i, MPUXXXX_LOG_TAG, "mpuxxxx driver inst start...\n");
    p_mpuxxxx_driver->private_data = g_mpuxxx_is_inited;

    mpuxxxx_status_t ret = MPUXXXX_OK;


    /************ 1.Checking input parameters ************/
    if (NULL == p_mpuxxxx_driver                         ||
        NULL == p_iic_driver_interface                   ||
        NULL == p_interrupt_interface                    ||
        NULL == p_timebase_interface                     ||
        NULL == p_delay_interface                        

#if OS_SUPPORTING
     || NULL == p_yield_interface                        ||
        NULL == p_os_interface                           
#endif // OS_SUPPORTING
    )
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx driver inst input error parameter\n");
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == p_iic_driver_interface->hi2c             ||
        NULL == p_iic_driver_interface->pf_iic_init      ||
        NULL == p_iic_driver_interface->pf_iic_deinit    ||
        NULL == p_iic_driver_interface->pf_iic_mem_write ||
        NULL == p_iic_driver_interface->pf_iic_mem_read  ||
        NULL == p_iic_driver_interface->\
                                     pf_iic_mem_read_dma)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx iic_driver_interface input error parameter\n");
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }

    if (
        // NULL == p_interrupt_interface->pf_irq_init          ||
        // NULL == p_interrupt_interface->pf_irq_deinit        ||
        NULL == p_interrupt_interface->pf_irq_enable        ||
        NULL == p_interrupt_interface->pf_irq_disable       ||
        NULL == p_interrupt_interface->pf_irq_clear_pending 
        // NULL == p_interrupt_interface->pf_irq_enable_clock  ||
        // NULL == p_interrupt_interface->pf_irq_disable_clock
       )
    {
#if MPUXXXX_DEBUG_ERR
        DEBUG_OUT(e, "mpuxxxx interrupt_interface input error parameter\n");
#endif // MPUXXXX_DEBUG_ERR
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }

    if (NULL == p_timebase_interface->pf_get_tick_count)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx timebase_interface input error parameter\n");
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }

    if (NULL == p_delay_interface->pf_delay_init         ||
        NULL == p_delay_interface->pf_delay_ms           ||
        NULL == p_delay_interface->pf_delay_us)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx delay_interface input error parameter\n");
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }

#if OS_SUPPORTING
    if (NULL == p_yield_interface->pf_rtos_yield)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx yield_interface input error parameter\n");
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }

    if (NULL == p_os_interface->pf_os_queue_create       ||
        NULL == p_os_interface->pf_os_mutex_create       ||
        NULL == p_os_interface->pf_os_semaphore_create   ||
        NULL == p_os_interface->pf_os_queue_delete       ||
        NULL == p_os_interface->pf_os_mutex_delete       ||
        NULL == p_os_interface->pf_os_semaphore_delete   ||
        NULL == p_os_interface->pf_os_mutex_lock         ||
        NULL == p_os_interface->pf_os_mutex_unlock       ||
        NULL == p_os_interface->pf_os_semaphore_take     ||
        NULL == p_os_interface->pf_os_semaphore_give)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
                  "mpuxxxx os_interface input error parameter\n");
        ret = MPUXXXX_ERRORPARAMETER;
        return ret;
    }
#endif // OS_SUPPORTING

    /************** 3.Mount the Interfaces ***************/
    // 3.1 mount external interfaces
    p_mpuxxxx_driver->p_iic_driver_instance       =     p_iic_driver_interface;
    p_mpuxxxx_driver->p_interrupt_instance        =      p_interrupt_interface;
    p_mpuxxxx_driver->p_timebase_instance         =       p_timebase_interface;
    p_mpuxxxx_driver->p_delay_instance            =          p_delay_interface;
#if OS_SUPPORTING
    p_mpuxxxx_driver->p_yield_instance = p_yield_interface;
    p_mpuxxxx_driver->p_os_instance    = p_os_interface;
#endif // OS_SUPPORTING
    // 3.2 mount internal interfaces    
    p_mpuxxxx_driver->pf_deinit                   =             mpuxxxx_deinit;
    p_mpuxxxx_driver->pf_sleep                    =              mpuxxxx_sleep;
    p_mpuxxxx_driver->pf_wakeup                   =             mpuxxxx_wakeup;
    p_mpuxxxx_driver->pf_set_gyro_fsr             =       mpuxxxx_set_gyro_fsr;
    p_mpuxxxx_driver->pf_set_accel_fsr            =      mpuxxxx_set_accel_fsr;
    p_mpuxxxx_driver->pf_set_lpf                  =            mpuxxxx_set_lpf;
    p_mpuxxxx_driver->pf_set_rate                 =           mpuxxxx_set_rate;
    p_mpuxxxx_driver->pf_set_interrupt_enable     =                           \
                                                  mpuxxxx_set_interrupt_enable;
    p_mpuxxxx_driver->pf_set_motion_threshold     =                           \
                                                  mpuxxxx_set_motion_threshold;
    p_mpuxxxx_driver->pf_set_INT_level            =      mpuxxxx_set_int_level;
    p_mpuxxxx_driver->pf_set_user_ctrl            =      mpuxxxx_set_user_ctrl;
    p_mpuxxxx_driver->pf_set_pwr_mgmt1_reg        =  mpuxxxx_set_pwr_mgmt1_reg;
    p_mpuxxxx_driver->pf_set_pwr_mgmt2_reg        =  mpuxxxx_set_pwr_mgmt2_reg;
    p_mpuxxxx_driver->pf_set_fifo_en_reg          =    mpuxxxx_set_fifo_en_reg;
    p_mpuxxxx_driver->pf_get_temperature          =    mpuxxxx_get_temperature;
    p_mpuxxxx_driver->pf_get_accel                =          mpuxxxx_get_accel;
    p_mpuxxxx_driver->pf_get_gyro                 =           mpuxxxx_get_gyro;
    p_mpuxxxx_driver->pf_get_all_data             =       mpuxxxx_get_all_data;
    p_mpuxxxx_driver->pf_get_interrupt_status_reg =                           \
                                              mpuxxxx_get_interrupt_status_reg;
    p_mpuxxxx_driver->pf_read_fifo_packet         =   mpuxxxx_read_fifo_packet;
    p_mpuxxxx_driver->pf_read_fifo_isr_occur      =mpuxxxx_read_fifo_isr_occur;   

    ret = bsp_mpuxxxx_driver_init(p_mpuxxxx_driver);
    if (ret != MPUXXXX_OK)
    {
        DEBUG_OUT(e, MPUXXXX_ERR_LOG_TAG,
            "mpuxxx driver inst fifo init failed\n");
            return ret;
    }
    
    // register interrupt callback after driver initialization to avoid 
    // interrupt occurs before initialization complete
    callback_register                                 (int_interrupt_callback);
    callback_register_dma                             (dma_interrupt_callback);

    g_mpuxxx_is_inited = MPUXXXX_IS_INITED;
    DEBUG_OUT(i, MPUXXXX_LOG_TAG, "mpuxxxx driver inst success\n");
    return ret;
}

//******************************* Declaring *********************************//
