/******************************************************************************
 * @file bsp_aht21_driver.c
 *
 * @par dependencies
 * - bsp_aht21_driver.h
 *
 * @author  Ethan-Hang
 *
 * @brief   AHT21 Temperature and Humidity Sensor Driver Implementation.
 * This file implements the AHT21 sensor driver functionality including:
 * - Sensor initialization and deinitialization
 * - Temperature and humidity measurement
 * - IIC communication protocol handling
 * - CRC8 data validation
 *
 * Processing flow:
 * 1. Initialize sensor with aht21_init()
 * 2. Read temperature/humidity with aht21_read_temp_humi()
 * 3. Optional power management with sleep/wakeup
 * @version V1.0 2025-11-27
 *
 * @note    1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_aht21_driver.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define AHT21_MEASURE_WATTING_TIME                       80
#define AHT21_NOT_INITED                              false
#define AHT21_INITED                                   true
#define AHT21_ID                                       0x18

#define CRC8_POLYNOMIAL                                0x31
#define CRC8_INITIAL                                   0xFF


static aht21_status_t aht21_read_status   (
                                           bsp_aht21_driver_t * const     this,
                                                      uint8_t * const rec_data
                                          );
#if USE_HARDWARE_I2C
static aht21_status_t __calibration_init  (bsp_aht21_driver_t * const     this);
#endif
//******************************** Defines **********************************//

//******************************* Variables *********************************//
static uint8_t g_device_id              =                 0;
//******************************* Variables *********************************//

/**
 * @brief Calculate CRC8 checksum
 *
 * This function calculates CRC8 checksum for data validation.
 * It uses polynomial 0x31 (CRC-8) with initial value 0xFF.
 *
 * @param[in] p_data Pointer to data buffer
 * @param[in] length Length of data in bytes
 *
 * @return uint8_t Calculated CRC8 checksum
 *
 * @note Used for data integrity verification in AHT21 communication
 */
static uint8_t CheckCrc8(uint8_t const *p_data, uint8_t const length)
{
    uint8_t crc = CRC8_INITIAL;

    for (uint8_t i = 0; i < length; i++)
    {
        crc ^= p_data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            }
            else
            {
                crc <<= (1);
            }
        }
    }

    return crc;
}

/**
 * @brief Read AHT21 device ID
 *
 * This function reads the device ID from AHT21 sensor using IIC
 * communication. It verifies if the connected device is indeed
 * an AHT21 sensor by checking the returned ID value.
 *
 * @param[in]  this Pointer to AHT21 driver instance
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK: Device ID read successfully
 *         - AHT21_ERRORPARAMETER: Invalid parameter
 *         - AHT21_ERRORRESOURCE: Device ID mismatch
 *
 * @note This function is called internally during initialization
 */
#if USE_HARDWARE_I2C
/**
 * @brief Initialize AHT21 calibration registers 0x1B / 0x1C / 0x1E.
 *
 * Called once at startup when the status byte indicates the sensor is not
 * yet calibrated ((status & 0x18) != 0x18). Writes a zero-argument reset
 * command to each register with a 10 ms gap, then returns so the caller
 * can re-verify the calibration bit.
 */
static aht21_status_t __calibration_init(bsp_aht21_driver_t * const this)
{
    aht21_status_t ret = AHT21_OK;
    uint8_t        cmd[3];

    static const uint8_t k_regs[3] = {AHT21_REG_CAL_1B, 
                                      AHT21_REG_CAL_1C, 
                                      AHT21_REG_CAL_1E};
    for (uint8_t i = 0; i < 3; i++)
    {
        cmd[0] = k_regs[i]; cmd[1] = 0x00; cmd[2] = 0x00;
        ret = this->p_iic_driver_instance->pf_i2c_master_write(
            NULL, (uint16_t)(AHT21_IIC_ADDR << 1), cmd, 3);
        if (AHT21_OK != ret)
        {
            DEBUG_OUT(e, AHT21_ERR_LOG_TAG, 
                     "aht21 calibration init failed at reg 0x%02X", k_regs[i]);
            return ret;
        }
#if OS_SUPPORTING
        this->p_yield_instance->pf_rtos_yield(10);
#endif // OS_SUPPORTING
    }

    return AHT21_OK;
}
#endif // USE_HARDWARE_I2C

static aht21_status_t __read_id(bsp_aht21_driver_t * const this)
{
    aht21_status_t ret  = AHT21_OK;
    uint8_t        data =        0;

    if (NULL == this)
    {
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }

#if (!USE_HARDWARE_I2C)
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_enter();
#endif
    // Send IIC Start Signal
    this->p_iic_driver_instance->pf_iic_start(NULL);

    // Send Command to read ID
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                              AHT21_REG_READ_ADDR);

    // Wait the ACK of I2C slave device
    if ( AHT21_OK == this->p_iic_driver_instance->pf_iic_wait_ack(NULL) )
    {
        this->p_iic_driver_instance->pf_iic_receive_byte(NULL, &data);
    }

    // Send the stop signal
    this->p_iic_driver_instance->pf_iic_stop(NULL);
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_exit();
#endif
#else
    ret = this->p_iic_driver_instance->pf_i2c_master_read(
        NULL, (uint16_t)(AHT21_IIC_ADDR << 1), &data, 1);
    if (AHT21_OK != ret)
    {
        return ret;
    }
#endif // !USE_HARDWARE_I2C

    if (AHT21_ID != (data & AHT21_ID))
    {
#if USE_HARDWARE_I2C
        /* Sensor not calibrated: initialize 0x1B/0x1C/0x1E and re-verify */
        ret = __calibration_init(this);
        if (AHT21_OK != ret)
        {
            return ret;
        }

        ret = this->p_iic_driver_instance->pf_i2c_master_read(
            NULL, (uint16_t)(AHT21_IIC_ADDR << 1), &data, 1);
        if (AHT21_OK != ret)
        {
            return ret;
        }

        if (AHT21_ID != (data & AHT21_ID))
        {
            DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 calibration init failed");
            ret = AHT21_ERRORRESOURCE;
            return ret;
        }
#else
        ret = AHT21_ERRORRESOURCE;
        return ret;
#endif // USE_HARDWARE_I2C
    }

    g_device_id = AHT21_ID;
    return ret;
}

/**
 * @brief Trigger a measurement on the AHT21 sensor.
 *
 * This function sends the measurement command sequence to the AHT21
 * device over I2C, waits the required conversion time and polls the
 * device status to ensure the measurement is finished. It implements
 * a small retry/timeout mechanism when the device remains busy.
 *
 * @param[in] this Pointer to AHT21 driver instance
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK on success
 *         - AHT21_ERRORPARAMETER if `this` is NULL
 *         - AHT21_ERRORTIMEOUT if measurement timed out
 */
static aht21_status_t __trigger_measurement(bsp_aht21_driver_t * const this)
{
    aht21_status_t ret      = AHT21_OK;
    uint8_t        rec_flag =        0;
    uint8_t        err_time =        5;


    if (NULL == this)
    {
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }

#if (!USE_HARDWARE_I2C)
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_enter();
#endif
    // Send IIC Start Signal
    this->p_iic_driver_instance->pf_iic_start(NULL);

    // Send address of I2C slave device
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                              AHT21_REG_WRITE_ADDR);
    this->p_iic_driver_instance->pf_iic_wait_ack(NULL);

    // Send the command to ask aht21 prepare data
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                              AHT21_REG_POINTER_AC);
    this->p_iic_driver_instance->pf_iic_wait_ack(NULL);

    // Send the command to configure aht21 parameter[1]
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                       AHT21_REG_MEASURE_CMD_ARGS1);
    this->p_iic_driver_instance->pf_iic_wait_ack(NULL);

    // Send the command to configure aht21 parameter[2]
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                       AHT21_REG_MEASURE_CMD_ARGS2);
    this->p_iic_driver_instance->pf_iic_wait_ack(NULL);

    // Send the stop signal
    this->p_iic_driver_instance->pf_iic_stop(NULL);
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_exit();
#endif
#else
    uint8_t cmd[3] = {
        AHT21_REG_POINTER_AC,
        AHT21_REG_MEASURE_CMD_ARGS1,
        AHT21_REG_MEASURE_CMD_ARGS2
    };
    ret = this->p_iic_driver_instance->pf_i2c_master_write(
        NULL, (uint16_t)(AHT21_IIC_ADDR << 1), cmd, 3);
    if (AHT21_OK != ret)
    {
        return ret;
    }
#endif // !USE_HARDWARE_I2C

// Wait for measurement to complete in AHT21_MEASURE_WATTING_TIME ms
#if OS_SUPPORTING
    this->p_yield_instance->pf_rtos_yield(AHT21_MEASURE_WATTING_TIME);
#else
    uint32_t start_time =  this->p_timebase_instance->pf_get_tick_count_ms();
    while (this->p_timebase_instance->pf_get_tick_count_ms() - 
                                    start_time < AHT21_MEASURE_WATTING_TIME)
    {
        ;
    }
#endif // OS_SUPPORTING
    ret = aht21_read_status(this, &rec_flag);

    while ( AHT21_OK ==              ret   &&
            0x80     == (rec_flag & 0x80)  &&
            err_time  >                0 )
    {
#if OS_SUPPORTING
        this->p_yield_instance->pf_rtos_yield(5);
#else
        uint32_t start_time = this->p_timebase_instance->\
                                          pf_get_tick_count_ms();
        while (this->p_timebase_instance->pf_get_tick_count_ms() -
                                        start_time < 5)
        {
            ;
        }
#endif // OS_SUPPORTING

        err_time--;
        if (0 == err_time)
        {
            DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 read temp humi timeout");
            ret = AHT21_ERRORTIMEOUT;
            return ret;
        }

        ret = aht21_read_status(this, &rec_flag);
    }
    return ret;
}

/**
 * @brief Read measurement bytes from AHT21 and validate CRC8.
 *
 * This function performs an I2C read sequence to obtain the six
 * measurement bytes from the AHT21 and the following CRC byte. The
 * six data bytes are returned via the output pointers. The function
 * computes CRC8 over the six bytes and compares it with the received
 * CRC; if the check fails an error is returned.
 *
 * @param[in]  this     Pointer to AHT21 driver instance
 * @param[out] byte_1th Pointer to store first data byte
 * @param[out] byte_2th Pointer to store second data byte
 * @param[out] byte_3th Pointer to store third data byte
 * @param[out] byte_4th Pointer to store fourth data byte
 * @param[out] byte_5th Pointer to store fifth data byte
 * @param[out] byte_6th Pointer to store sixth data byte
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK on success
 *         - AHT21_ERRORPARAMETER on NULL pointer
 *         - AHT21_ERROR if CRC validation fails or reception error
 */
static aht21_status_t __start_receive(
                                      bsp_aht21_driver_t * const     this,
                                      uint8_t            * const byte_1th,
                                      uint8_t            * const byte_2th,
                                      uint8_t            * const byte_3th,
                                      uint8_t            * const byte_4th,
                                      uint8_t            * const byte_5th,
                                      uint8_t            * const byte_6th)
{
    aht21_status_t ret = AHT21_OK;
    uint8_t   byte_crc =        0;

    if (
        NULL ==     this      ||
        NULL == byte_1th      ||
        NULL == byte_2th      ||
        NULL == byte_3th      ||
        NULL == byte_4th      ||
        NULL == byte_5th      ||
        NULL == byte_6th)
    {
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }

#if (!USE_HARDWARE_I2C)
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_enter();
#endif
    // Send IIC Start Signal
    this->p_iic_driver_instance->pf_iic_start(NULL);

    // Send address of I2C slave device
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                              AHT21_REG_READ_ADDR);
    this->p_iic_driver_instance->pf_iic_wait_ack(NULL);

    // Read 6 bytes data from AHT21
    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 byte_1th);
    this->p_iic_driver_instance->pf_iic_send_ack(NULL);

    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 byte_2th);
    this->p_iic_driver_instance->pf_iic_send_ack(NULL);

    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 byte_3th);
    this->p_iic_driver_instance->pf_iic_send_ack(NULL);

    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 byte_4th);
    this->p_iic_driver_instance->pf_iic_send_ack(NULL);

    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 byte_5th);
    this->p_iic_driver_instance->pf_iic_send_ack(NULL);

    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 byte_6th);
    this->p_iic_driver_instance->pf_iic_send_ack(NULL);

    // Read CRC byte
    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                &byte_crc);
    this->p_iic_driver_instance->pf_iic_send_no_ack(NULL);

    // Send the stop signal
    this->p_iic_driver_instance->pf_iic_stop(NULL);
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_exit();
#endif
#else
    /* Hardware I2C: read all 7 bytes (6 data + 1 CRC) in one transaction */
    uint8_t rx_buf[7] = {0};
    ret = this->p_iic_driver_instance->pf_i2c_master_read(
        NULL, (uint16_t)(AHT21_IIC_ADDR << 1), rx_buf, 7);
    if (AHT21_OK != ret)
    {
        return ret;
    }
    *byte_1th = rx_buf[0];
    *byte_2th = rx_buf[1];
    *byte_3th = rx_buf[2];
    *byte_4th = rx_buf[3];
    *byte_5th = rx_buf[4];
    *byte_6th = rx_buf[5];
     byte_crc = rx_buf[6];
#endif // USE_HARDWARE_I2C    

    // CRC Check
    uint8_t data_for_crc[6] = 
    {
        *byte_1th,  
        *byte_2th,
        *byte_3th,  
        *byte_4th,
        *byte_5th,  
        *byte_6th 
    };
    uint8_t crc_cal = CheckCrc8(data_for_crc, 6);
    if (crc_cal != byte_crc)
    {
        ret = AHT21_ERROR;
        return ret;
    }

    return ret;
}

/**
 * @brief Read AHT21 sensor status
 *
 * This function reads the current status byte from AHT21 sensor.
 * The status byte contains information about measurement status,
 * calibration status, and busy flag.
 *
 * @param[in]  this     Pointer to AHT21 driver instance
 * @param[out] rec_data Pointer to store the status byte
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK: Status read successfully
 *         - AHT21_ERRORPARAMETER: Invalid parameter
 *
 * @note Status byte bit[7] indicates busy flag (1=busy, 0=ready)
 */
static aht21_status_t aht21_read_status(bsp_aht21_driver_t * const     this,
                                                   uint8_t * const rec_data)
{
    aht21_status_t ret = AHT21_OK;

    if (NULL == this || NULL == rec_data)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21_read_status invalid parameter");
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }

#if (!USE_HARDWARE_I2C)
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_enter();
#endif
    // Send IIC Start Signal
    this->p_iic_driver_instance->pf_iic_start(NULL);

    // Send address for reading status
    this->p_iic_driver_instance->pf_iic_send_byte(NULL,
                                              AHT21_REG_READ_ADDR);
    this->p_iic_driver_instance->pf_iic_wait_ack(NULL);

    // Receive status byte
    this->p_iic_driver_instance->pf_iic_receive_byte(NULL,
                                                 rec_data);

    // Send NO ACK and stop signal
    this->p_iic_driver_instance->pf_iic_send_no_ack(NULL);

    this->p_iic_driver_instance->pf_iic_stop(NULL);
#if (OS_SUPPORTING)
    this->p_iic_driver_instance->pf_critical_exit();
#endif
#else
    ret = this->p_iic_driver_instance->pf_i2c_master_read(
        NULL, (uint16_t)(AHT21_IIC_ADDR << 1), rec_data, 1);
    if (AHT21_OK != ret)
    {
        return ret;
    }
#endif // USE_HARDWARE_I2C
    DEBUG_OUT(d, AHT21_LOG_TAG, "mesurment finished, read return data: 0x%02X", *rec_data);
    return ret;
}

/**
 * @brief Initialize AHT21 sensor
 *
 * This function initializes the AHT21 sensor by performing the
 * following steps:
 * 1. Power-on delay (100ms)
 * 2. IIC interface initialization
 * 3. Device ID verification
 * 4. Set initialization flag
 *
 * @param[in] this Pointer to AHT21 driver instance
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK: Initialization successful
 *         - AHT21_ERRORPARAMETER: Invalid parameter
 *         - AHT21_ERRORRESOURCE: Device ID mismatch
 *
 * @note Must be called before any sensor operations
 */
static aht21_status_t aht21_init (bsp_aht21_driver_t * const this)
{
    DEBUG_OUT(i, AHT21_LOG_TAG, "aht21_init start");
    aht21_status_t ret = AHT21_OK;
    /************* 1.Checking input parameters ***********/
    if (NULL == this)
    {
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }
    // instance in this pointer already checked

    /**************** 2. Start Aht21 Init ****************/
    // 2.1 Delay 100ms for sensor init
#if OS_SUPPORTING
    this->p_yield_instance->pf_rtos_yield(100);
#else
    uint32_t start_time =  this->p_timebase_instance->pf_get_tick_count_ms();
    while (this->p_timebase_instance->pf_get_tick_count_ms() - 
                                              start_time < 100)
    {
        ;
    }
#endif // OS_SUPPORTING

    // 2.1.1 Enter Critical Section
#if ((!USE_HARDWARE_I2C) && (OS_SUPPORTING))
    this->p_iic_driver_instance->pf_critical_enter();
#endif // ((!USE_HARDWARE_I2C) & (OS_SUPPORTING))

    // 2.1.2 I2C Init
    this->p_iic_driver_instance->pf_iic_init(NULL);

    // 2.1.3 Read the Id Internally
    ret = __read_id(this);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 read id failed");
        return ret;
    }

    // 2.1.Exit Critical Section
#if ((!USE_HARDWARE_I2C) && (OS_SUPPORTING))
    this->p_iic_driver_instance->pf_critical_exit();
#endif // ((!USE_HARDWARE_I2C) & (OS_SUPPORTING))

    // Change the Flag of the internal flag
    this->aht21_is_init = AHT21_INITED;
    DEBUG_OUT(d, AHT21_LOG_TAG, "aht21_init success, device id: 0x%02X", g_device_id);

    return ret;
}

/**
 * @brief Deinitialize AHT21 sensor
 *
 * This function deinitializes the AHT21 sensor by clearing the
 * initialization flag. It does not perform any hardware operations.
 *
 * @param[in] this Pointer to AHT21 driver instance
 *
 * @return aht21_status_t Always returns AHT21_OK
 *
 * @note This function only clears the internal initialization flag
 */
static aht21_status_t aht21_deinit (bsp_aht21_driver_t  * const this)
{
    aht21_status_t ret = AHT21_OK;
    this->aht21_is_init = AHT21_NOT_INITED;
    DEBUG_OUT(d, AHT21_LOG_TAG, "aht21_deinit success");

    return ret;
}

/**
 * @brief Read stored AHT21 device ID
 *
 * This function returns the device ID that was read during
 * initialization. It does not perform IIC communication.
 *
 * @param[in]  this   Pointer to AHT21 driver instance
 * @param[out] p_addr Pointer to store device ID
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK: Device ID read successfully
 *         - AHT21_ERRORRESOURCE: Driver not initialized
 *
 * @note This function returns the cached ID from initialization
 */
static aht21_status_t aht21_read_id (bsp_aht21_driver_t  * const   this,
                                               uint32_t  * const p_addr)
{
    aht21_status_t ret = AHT21_OK;

    if (NULL == this || NULL == p_addr)
    {
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }

    if (!this->aht21_is_init)
    {
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

    *p_addr = g_device_id;
    DEBUG_OUT(d, AHT21_LOG_TAG, "aht21_read_id device id: 0x%02X", g_device_id);
    return ret;
}

/**
 * @brief Read temperature and humidity from AHT21 sensor
 *
 * This function performs a complete measurement cycle:
 * 1. Send measurement command to sensor
 * 2. Wait for measurement completion (80ms + status polling)
 * 3. Read 6 bytes of measurement data
 * 4. Convert raw data to temperature and humidity values
 *
 * @param[in]  this   Pointer to AHT21 driver instance
 * @param[out] p_temp Pointer to store temperature value (°C)
 * @param[out] p_humi Pointer to store humidity value (%RH)
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK: Measurement successful
 *         - AHT21_ERRORRESOURCE: Driver not initialized
 *         - AHT21_ERRORTIMEOUT: Measurement timeout
 *
 * @note Temperature range: -40°C to +85°C
 * @note Humidity range: 0% to 100% RH
 */
static aht21_status_t aht21_read_temp_humi
                                       (
                                         bsp_aht21_driver_t * const   this,
                                                      float * const p_temp,
                                                      float * const p_humi
                                       )
{
    // Define Receive data variables
    uint8_t     byte_1th = 0;
    uint8_t     byte_2th = 0;
    uint8_t     byte_3th = 0;
    uint8_t     byte_4th = 0;
    uint8_t     byte_5th = 0;
    uint8_t     byte_6th = 0;
    int32_t     res_data = 0;

    aht21_status_t ret = AHT21_OK;
    
    if (!this->aht21_is_init)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21_read_temp_humi driver not inited");
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

    // Trigger Measurement
    ret = __trigger_measurement(this);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 trigger measure failed");
        return ret;
    }
  
    // Start to receive data
    DEBUG_OUT(d, AHT21_LOG_TAG, "start to read measurement data");
    ret = __start_receive(     this,
                          &byte_1th,
                          &byte_2th,
                          &byte_3th,
                          &byte_4th,
                          &byte_5th,
                          &byte_6th);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 receive data failed");
        return ret;
    }

    // Calculate Humidity and Temperature
    res_data  =  0x00; 
    res_data  = ( res_data | byte_2th ) << (8);
    res_data  = ( res_data | byte_3th ) << (8);
    res_data  = ( res_data | byte_4th ) >> (4);
    *p_humi   = ( res_data * 1000 ) >> (20);
    *p_humi   = ( *p_humi ) / 10.0f;
                
    res_data  =  0x00;
    res_data  = ( res_data | (byte_4th & 0x0F) ) << (8);
    res_data  = ( res_data | byte_5th ) << (8);
    res_data  = ( res_data | byte_6th );
    res_data  =   res_data & 0xFFFFF;
    *p_temp   = ( res_data  * 2000 ) >> (20);
    *p_temp   = ( *p_temp ) / 10.0f - 50.0f;

    return ret;
}

/**
 * @brief Read humidity from AHT21 sensor (unsupported)
 *
 * This function is currently not implemented as AHT21 sensor
 * only supports combined temperature and humidity measurement.
 *
 * @param[in]  this  Pointer to AHT21 driver instance
 * @param[out] p_humi Pointer to store humidity value (%RH)
 *
 * @return aht21_status_t Always returns AHT21_ERRORUNSUPPORTED
 *
 * @note Use aht21_read_temp_humi() for combined measurement
 */
static aht21_status_t aht21_read_humi  (
                                         bsp_aht21_driver_t * const   this,
                                                      float * const p_humi
                                       )
{
    // Define Receive data variables
    uint8_t     byte_1th = 0;
    uint8_t     byte_2th = 0;
    uint8_t     byte_3th = 0;
    uint8_t     byte_4th = 0;
    uint8_t     byte_5th = 0;
    uint8_t     byte_6th = 0;
    int32_t     res_data = 0;
    
    aht21_status_t ret = AHT21_OK;

    if (!this->aht21_is_init)
    {
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

    ret = __trigger_measurement(this);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 trigger measure failed");
        return ret;
    }

    ret = __start_receive(     this,
                          &byte_1th,
                          &byte_2th,
                          &byte_3th,
                          &byte_4th,
                          &byte_5th,
                          &byte_6th);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 receive data failed");
        return ret;
    }

    // Calculate Humidity
    res_data  =  0x00;
    res_data  = ( res_data | byte_2th ) << (8);
    res_data  = ( res_data | byte_3th ) << (8);
    res_data  = ( res_data | byte_4th ) >> (4);
    *p_humi   = ( res_data * 1000 ) >> (20);
    *p_humi   = ( *p_humi ) / 10.0f;

    return ret;
}

/**
 * @brief Read temperature (degrees Celsius) from the AHT21 sensor.
 *
 * This function triggers a single measurement, reads the raw sensor
 * data and converts the temperature raw value to degrees Celsius.
 * It validates the driver initialization and returns appropriate
 * error codes on failure.
 *
 * @param[in]  this   Pointer to AHT21 driver instance
 * @param[out] p_temp Pointer to store temperature value (°C)
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK on success
 *         - AHT21_ERRORRESOURCE if driver not initialized
 *         - Other error codes on communication failure
 */
static aht21_status_t aht21_read_temp  (
                                         bsp_aht21_driver_t * const   this,
                                                      float * const p_temp
                                       )
{
    // Define Receive data variables
    uint8_t     byte_1th = 0;
    uint8_t     byte_2th = 0;
    uint8_t     byte_3th = 0;
    uint8_t     byte_4th = 0;
    uint8_t     byte_5th = 0;
    uint8_t     byte_6th = 0;
    int32_t     res_data = 0;
    
    aht21_status_t ret = AHT21_OK;

    if (!this->aht21_is_init)
    {
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

    ret = __trigger_measurement(this);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 trigger measure failed");
        return ret;
    }

    ret = __start_receive(     this,
                          &byte_1th,
                          &byte_2th,
                          &byte_3th,
                          &byte_4th,
                          &byte_5th,
                          &byte_6th);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 receive data failed");
        return ret;
    }

    // Calculate Temperature
    res_data  =  0x00;
    res_data  = ( res_data | (byte_4th & 0x0F) ) << (8);
    res_data  = ( res_data | byte_5th ) << (8);
    res_data  = ( res_data | byte_6th );
    res_data  =   res_data & 0xFFFFF;
    *p_temp   = ( res_data  * 2000 ) >> (20);
    *p_temp   = ( *p_temp ) / 10.0f - 50.0f;

    return ret;
}

/**
 * @brief Put AHT21 sensor to sleep mode (unsupported)
 *
 * This function is currently not implemented as the project
 * does not currently have low power requirements.
 *
 * @param[in] this Pointer to AHT21 driver instance
 *
 * @return aht21_status_t Always returns AHT21_ERRORUNSUPPORTED
 *
 * @note This feature may be implemented in future versions
 */
static aht21_status_t aht21_sleep (bsp_aht21_driver_t * const this)
{
    aht21_status_t ret = AHT21_ERRORUNSUPPORTED;
    // if (!this->aht21_is_init)
    // {
    //     ret = AHT21_ERRORRESOURCE;
    //     return ret;
    // }
    return ret;
}

/**
 * @brief Wake up AHT21 sensor from sleep mode (unsupported)
 *
 * This function is currently not implemented as the project
 * does not currently have low power requirements.
 *
 * @param[in] this Pointer to AHT21 driver instance
 *
 * @return aht21_status_t Always returns AHT21_ERRORUNSUPPORTED
 *
 * @note This feature may be implemented in future versions
 */
static aht21_status_t aht21_wakeup (bsp_aht21_driver_t * const this)
{
    aht21_status_t ret = AHT21_ERRORUNSUPPORTED;
    // if (!this->aht21_is_init)
    // {
    //     ret = AHT21_ERRORRESOURCE;
    //     return ret;
    // }
    return ret;
}

/**
 * @brief Create and initialize an AHT21 driver instance
 *
 * This function creates a new AHT21 driver instance and initializes
 * it with the provided interface implementations. It sets up the
 * driver's function pointers for sensor operations.
 *
 * @param[out] p_aht21_inst      Pointer to AHT21 driver instance
 * @param[in]  p_iic_driver_inst Pointer to IIC driver interface
 * @param[in]  p_yield_inst      Pointer to RTOS yield interface
 * @param[in]  p_timebase_inst   Pointer to timebase interface
 * @param[in]  p_irq_instance    Pointer to IRQ interface
 *
 * @return aht21_status_t Operation status:
 *         - AHT21_OK: Instance creation successful
 *         - AHT21_ERRORPARAMETER: Invalid parameter
 *         - AHT21_ERRORRESOURCE: Interface validation failed
 *
 * @note Call before using other AHT21 driver functions
 * @note Initialize with pf_init() after creation
 */
aht21_status_t aht21_inst(
        bsp_aht21_driver_t            *const        p_aht21_inst,
        aht21_iic_driver_interface_t  *const   p_iic_driver_inst,
#if OS_SUPPORTING
        aht21_yield_interface_t       *const        p_yield_inst,
#endif // OS_SUPPORTING
        aht21_timebase_interface_t    *const     p_timebase_inst
                         )
{
    DEBUG_OUT(i, AHT21_LOG_TAG, "aht21_inst start");

    aht21_status_t ret = AHT21_OK;
    /************ 1.Checking input parameters ************/
    if ( 
         NULL == p_aht21_inst      ||
         NULL == p_iic_driver_inst ||
         NULL == p_timebase_inst
#if OS_SUPPORTING
      || NULL == p_yield_inst
#endif // OS_SUPPORTING
       )
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21_inst input error parameter");
        ret = AHT21_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (AHT21_INITED == p_aht21_inst->aht21_is_init)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 instance already initialized");
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

    /******** 3.Mount and Checking the Interfaces ********/
    // 3.1 mount external interfaces
    p_aht21_inst->p_iic_driver_instance = p_iic_driver_inst;
    if (
        NULL == p_aht21_inst->p_iic_driver_instance                          ||
        NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_init             ||
        NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_deinit
#if USE_HARDWARE_I2C
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_i2c_master_write
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_i2c_master_read
#else
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_receive_byte
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_send_byte
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_send_ack
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_send_no_ack
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_start
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_stop
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_iic_wait_ack
#if OS_SUPPORTING
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_critical_enter
     || NULL == p_aht21_inst->p_iic_driver_instance->pf_critical_exit
#endif // OS_SUPPORTING
#endif // USE_HARDWARE_I2C
       )
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 iic driver instance error");
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

    p_aht21_inst->p_timebase_instance   =  p_timebase_inst;
    if (
        NULL == p_aht21_inst->p_timebase_instance                  ||
        NULL == p_aht21_inst->p_timebase_instance->\
                                     pf_get_tick_count_ms
       )
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 timebase instance error");
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }

#if OS_SUPPORTING
    p_aht21_inst->p_yield_instance     =      p_yield_inst;
    if (
        NULL == p_aht21_inst->p_yield_instance                     ||
        NULL == p_aht21_inst->p_yield_instance->pf_rtos_yield
       )
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, "aht21 yield instance error");
        ret = AHT21_ERRORRESOURCE;
        return ret;
    }
#endif // OS_SUPPORTING

    // 3.2 mount internal interfaces
    p_aht21_inst->pf_init           =           aht21_init;
    p_aht21_inst->pf_deinit         =         aht21_deinit;
    p_aht21_inst->pf_read_id        =        aht21_read_id;
    p_aht21_inst->pf_read_temp_humi = aht21_read_temp_humi;
    p_aht21_inst->pf_read_humi      =      aht21_read_humi;
    p_aht21_inst->pf_read_temp      =      aht21_read_temp;
    p_aht21_inst->pf_sleep          =          aht21_sleep;
    p_aht21_inst->pf_wakeup         =         aht21_wakeup;

    ret = aht21_init(p_aht21_inst);
    if (AHT21_OK != ret)
    {
        DEBUG_OUT(e, AHT21_ERR_LOG_TAG, 
                             "aht21 instance init failed");
        return ret;
    }

    DEBUG_OUT(d, AHT21_LOG_TAG, "aht21_inst success");
    return ret;
}
