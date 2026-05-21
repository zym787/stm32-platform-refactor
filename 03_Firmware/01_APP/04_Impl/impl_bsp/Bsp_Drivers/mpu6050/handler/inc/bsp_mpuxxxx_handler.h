/******************************************************************************
 * @file bsp_mpuxxxx_handler.h
 *
 * @par dependencies
 * - bsp_mpuxxxx_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief Declare MPUXXXX handler context and task-level integration APIs.
 *
 * Processing flow:
 * Bind external interfaces, initialize handler resources, then run task loop.
 * @version V1.0 2026-3-2
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#ifndef __BSP_MPUXXXX_HANLDER_H__
#define __BSP_MPUXXXX_HANLDER_H__

//******************************** Includes *********************************//
#include "bsp_mpuxxxx_driver.h"
#include "osal_common_types.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef struct
{
    iic_driver_interface_t         const * const      p_iic_driver;
    timebase_interface_t           const * const        p_timebase;
    delay_interface_t              const * const p_delay_interface;
    yield_interface_t              const * const p_yield_interface;
    os_interface_t                 const * const    p_os_interface;
    hardware_interrupt_interface_t const * const \
                                             p_interrupt_interface;
} mpuxxxx_handler_input_args_t;

typedef struct
{
    mpuxxxx_handler_input_args_t  
                           const *            p_input_args;
    bsp_mpuxxxx_driver_t         *                p_driver;
    void                         *  p_unpack_queue_handler;
    uint32_t                               queue_item_size;
    uint32_t                                  queue_length;
}bsp_mpuxxxx_hanlder_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief   Instantiate the MPU handler.
 *          Binds the input arguments to the handler
 *          and triggers the internal init routine.
 * 
 * @param[in] p_handler    : Pointer to the handler
 *                           instance.
 * @param[in] p_input_args : Pointer to the init args,
 *                           including OS, IIC, delay
 *                           and yield interfaces.
 * 
 * @param[out] : None
 * 
 * @return  MPUXXXX_OK             - Success
 *          MPUXXXX_ERRORPARAMETER - NULL pointer input
 *          Other                  - Init error
 * 
 * */
mpuxxxx_status_t mpuxxxx_hanlder_inst(bsp_mpuxxxx_hanlder_t      *   p_handler,
                                    mpuxxxx_handler_input_args_t *p_input_args);

/**
 * @brief   Get a read-only pointer to the singleton handler instance.
 *          External modules must call this instead of using extern.
 *
 * @return  Const pointer to the handler instance.
 *
 * */
bsp_mpuxxxx_hanlder_t const * mpuxxxx_handler_get_instance(void);

                                    /**
 * @brief   FreeRTOS task for the MPU handler.
 *          Initializes the circular buffer, driver
 *          and handler instance. Runs a polling loop
 *          that monitors the DMA completion flag;
 *          on flag asserted, sends a notification
 *          to the unpack queue.
 * 
 * @param[in] argument : Pointer to
 *                       mpuxxxx_handler_input_args_t
 *                       containing all required OS,
 *                       IIC, delay and yield
 *                       interfaces.
 * 
 * @param[out] : None
 * 
 * @return  None
 * 
 * */
void mpuxxxx_handler_thread(void *argument);

//******************************* Declaring *********************************//

#endif // end of __BSP_MPUXXXX_HANLDER_H__
