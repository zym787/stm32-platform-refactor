/******************************************************************************
 * @file bsp_adapter_port_motion.c
 *
 * @par dependencies
 * - bsp_mpuxxxx_handler.h
 * - bsp_adapter_port_motion.h
 *
 * @author Ethan-Hang
 *
 * @brief MPU6050 adapter implementation for the motion wrapper layer.
 *        Implements the motion_drv_t vtable using the MPU6050 handler
 *        and circular buffer APIs, then registers itself into the wrapper.
 *
 * Processing flow:
 *   mpuxxxx_drv_get_req()
 *     → spins until the handler OS queue is ready
 *     → blocks on pf_os_queue_receive() until a DMA-complete notification
 *       arrives from the handler task
 *   mpuxxxx_get_data_addr()
 *     → returns the current read-slot address from the circular buffer
 *   mpuxxxx_read_data_done()
 *     → advances the circular buffer read pointer
 *
 * @version V1.0 2026-04-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_mpuxxxx_handler.h"
#include "bsp_adapter_port_motion.h"

#include "osal_common_types.h"
#include "osal_task.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//

/**
 * @brief   Adapter init — no hardware action needed; MPU6050 is initialised
 *          by its own handler task.
 *
 * @param[in] dev : Pointer to the motion driver vtable (unused).
 */
static void mpuxxxx_drv_init(motion_drv_t *const dev)
{
    (void)dev;
}

/**
 * @brief   Adapter deinit — no hardware action needed here.
 *
 * @param[in] dev : Pointer to the motion driver vtable (unused).
 */
static void mpuxxxx_drv_deinit(motion_drv_t *const dev)
{
    (void)dev;
}

/**
 * @brief   Block until a new DMA data packet is available.
 *          Spins (with a yield) while the handler OS queue has not yet been
 *          created, then performs a blocking receive on that queue.
 *
 * @param[in] dev : Pointer to the motion driver vtable (unused).
 *
 * @return  WP_MOTION_OK           - A new packet notification was received.
 *          WP_MOTION_ERRORRESOURCE - Handler not ready (NULL interface ptr).
 */
static wp_motion_status_t mpuxxxx_drv_get_req(motion_drv_t *const dev)
{
    (void)dev;
    bsp_mpuxxxx_hanlder_t const *p_handler = mpuxxxx_handler_get_instance();
    mpuxxxx_status_t             ret       = MPUXXXX_OK;

    /* Wait until the handler is initialized */
    while (NULL == p_handler)
    {
        p_handler = mpuxxxx_handler_get_instance();
        osal_task_delay(50);
    }

    if (NULL == p_handler->p_input_args                                      ||
        NULL == p_handler->p_input_args->p_os_interface                      ||
        NULL == p_handler->p_input_args->p_os_interface->pf_os_queue_receive ||
        NULL == p_handler->p_input_args->p_yield_interface                   ||
        NULL == p_handler->p_input_args->p_yield_interface->pf_rtos_yield)
    {
        return WP_MOTION_ERRORRESOURCE;
    }

    /* Blocking receive — releases only when the handler sends a notification.
     * A local dummy buffer is required; the payload is a 1-byte token
     * whose value is irrelevant (notification only). */
    uint32_t dummy = 0;
    ret = p_handler->p_input_args->p_os_interface->pf_os_queue_receive(
                                          p_handler->p_unpack_queue_handler,
                                                                     &dummy,
                                                             OSAL_MAX_DELAY);
    if (MPUXXXX_OK != ret)
    {
        DEBUG_OUT(e, UNPACK_ERR_LOG_TAG,
                    "mpuxxxx unpack task receive queue failed!");
    }

    return WP_MOTION_OK;
}

/**
 * @brief   Return the address of the current raw data read-slot.
 *          The caller must not advance the read pointer until
 *          mpuxxxx_read_data_done() is called.
 *
 * @param[in] dev : Pointer to the motion driver vtable (unused).
 *
 * @return  Pointer to the 14-byte MPU6050 data packet, or NULL on error.
 */
static uint8_t *mpuxxxx_get_data_addr(motion_drv_t *const dev)
{
    (void)dev;
    circular_buffer_t *p_cbuf = circular_buffer_get_instance();
    return p_cbuf->pf_get_rbuffer_addr(p_cbuf);
}

/**
 * @brief   Advance the circular buffer read pointer after data is consumed.
 *
 * @param[in] dev : Pointer to the motion driver vtable (unused).
 */
static void mpuxxxx_read_data_done(motion_drv_t *const dev)
{
    (void)dev;
    circular_buffer_t *p_cbuf = circular_buffer_get_instance();
    p_cbuf->pf_data_readed(p_cbuf);
}

/**
 * @brief   Register the MPU6050 driver into the motion wrapper.
 *          Fills the vtable and calls drv_adapter_motion_mount().
 *
 * @return  true  - Registration successful.
 *          false - Mount failed.
 */
bool drv_adapter_motion_register(void)
{
    motion_drv_t motion_drv = {
        .dev_id                    =                       0,
        .idx                       =                       0,
        .user_data                 =                    NULL,
        .pf_motion_drv_init        =        mpuxxxx_drv_init,
        .pf_motion_drv_deinit      =      mpuxxxx_drv_deinit,
        .pf_motion_drv_get_req     =     mpuxxxx_drv_get_req,
        .pf_motion_get_data_addr   =   mpuxxxx_get_data_addr,
        .pf_motion_read_data_done  =  mpuxxxx_read_data_done,
    };

    return drv_adapter_motion_mount(0, &motion_drv);
}

//******************************* Functions *********************************//
