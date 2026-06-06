/******************************************************************************
 * @file mpu6050_unpack.c
 *
 * @par dependencies
 * - bsp_wrapper_motion.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief FreeRTOS task that unpacks raw MPU6050 DMA packets.
 *        Uses the motion wrapper API; has no direct dependency on the
 *        MPU6050 driver or circular buffer implementation.
 *
 * Processing flow:
 *   1. motion_drv_get_req() blocks until a DMA packet is ready.
 *   2. motion_get_data_addr() retrieves the raw 14-byte packet address.
 *   3. Accel / gyro / temperature fields are extracted and converted.
 *   4. motion_read_data_done() advances the ring buffer read pointer.
 *
 * Packet layout (MPU6050, 14 bytes):
 *   [0:1]   ACCEL_X  [2:3]   ACCEL_Y  [4:5]   ACCEL_Z
 *   [6:7]   TEMP
 *   [8:9]   GYRO_X   [10:11] GYRO_Y   [12:13] GYRO_Z
 *
 * @version V1.0 2026-04-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_motion.h"
#include "platform_type.h"

#include "osal_task.h"

#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/*
 * Keep MPU data shape local to APP layer to avoid exposing driver typedefs.
 */
typedef struct
{
    INT16_T temperature_raw;

    INT16_T accel_x_raw;
    INT16_T accel_y_raw;
    INT16_T accel_z_raw;

    INT16_T gyro_x_raw;
    INT16_T gyro_y_raw;
    INT16_T gyro_z_raw;

    FLOAT32_T temperature_c;

    FLOAT32_T accel_x_g;
    FLOAT32_T accel_y_g;
    FLOAT32_T accel_z_g;

    FLOAT32_T gyro_x_dps;
    FLOAT32_T gyro_y_dps;
    FLOAT32_T gyro_z_dps;
} mpuxxxx_unpack_data_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//

/**
 * @brief   FreeRTOS unpack task for MPU6050 motion data.
 *          Loops indefinitely: waits for a DMA-complete notification,
 *          parses the raw packet, prints the result, then releases the slot.
 *
 * @param[in] argument : Unused task argument.
 */
void unpack_task_thread(void *argument)
{
    // osal_task_delay(200);
    (void)argument;

    DEBUG_OUT(i, UNPACK_LOG_TAG, "unpack_task is running...");

    platform_err_t ret         = PLATFORM_OK;
    mpuxxxx_unpack_data_t     motion_data = {0};

    for (;;)
    {
        /* Block until the handler task sends a DMA-complete notification. */
        ret = motion_drv_get_req();
        if (PLATFORM_OK != ret)
        {
            DEBUG_OUT(e, UNPACK_ERR_LOG_TAG,
                      "motion_drv_get_req failed, ret=%d", (int)ret);
            continue;
        }

        /* Obtain a pointer to the raw 14-byte packet. */
        UINT8_T *addr = motion_get_data_addr();
        if (NULL == addr)
        {
            DEBUG_OUT(e, UNPACK_ERR_LOG_TAG,
                      "motion_get_data_addr returned NULL");
            motion_read_data_done();
            continue;
        }

        DEBUG_OUT(i, UNPACK_LOG_TAG, "circular buffer: addr = %p", addr);

        /* ---- Parse raw bytes (big-endian, MSB first) ---- */
        motion_data.accel_x_raw     = (INT16_T)(*(addr + 0) << 8 | *(addr + 1));
        motion_data.accel_y_raw     = (INT16_T)(*(addr + 2) << 8 | *(addr + 3));
        motion_data.accel_z_raw     = (INT16_T)(*(addr + 4) << 8 | *(addr + 5));
        motion_data.temperature_raw = (INT16_T)(*(addr + 6) << 8 | *(addr + 7));
        motion_data.gyro_x_raw      = (INT16_T)(*(addr + 8) << 8 | *(addr + 9));
        motion_data.gyro_y_raw      = (INT16_T)(*(addr + 10) << 8 | *(addr + 11));
        motion_data.gyro_z_raw      = (INT16_T)(*(addr + 12) << 8 | *(addr + 13));

        /* ---- Convert to engineering units ---- */
        motion_data.temperature_c =
            36.53f + (FLOAT32_T)motion_data.temperature_raw / 340.0f;

        motion_data.accel_x_g  = (FLOAT32_T)motion_data.accel_x_raw / 16384.0f;
        motion_data.accel_y_g  = (FLOAT32_T)motion_data.accel_y_raw / 16384.0f;
        motion_data.accel_z_g  = (FLOAT32_T)motion_data.accel_z_raw / 16384.0f;

        motion_data.gyro_x_dps = (FLOAT32_T)motion_data.gyro_x_raw / 131.0f;
        motion_data.gyro_y_dps = (FLOAT32_T)motion_data.gyro_y_raw / 131.0f;
        motion_data.gyro_z_dps = (FLOAT32_T)motion_data.gyro_z_raw / 131.0f;

        DEBUG_OUT(i, UNPACK_LOG_TAG,
                  "  TEMP : %4.2f C\r\n"
                  "  ACC  : X=%+5.3f g   Y=%+5.3f g   Z=%+5.3f g\r\n"
                  "  GYRO : X=%+5.3f dps Y=%+5.3f dps Z=%+5.3f dps",
                  (FLOAT64_T)motion_data.temperature_c,
                  (FLOAT64_T)motion_data.accel_x_g, (FLOAT64_T)motion_data.accel_y_g,
                  (FLOAT64_T)motion_data.accel_z_g, (FLOAT64_T)motion_data.gyro_x_dps,
                  (FLOAT64_T)motion_data.gyro_y_dps,
                  (FLOAT64_T)motion_data.gyro_z_dps);

        /* Release the circular buffer slot so the writer can reuse it. */
        motion_read_data_done();
    }
}

//******************************* Functions *********************************//
