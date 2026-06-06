/******************************************************************************
 * @file user_isr_handlers.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Bridge HAL IRQ callbacks to MPU driver interrupt handlers.
 *
 * Processing flow:
 * Forward EXTI and I2C DMA callbacks through registered function pointers.
 * @version V1.0 2026--
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "main.h"
#include "platform_type.h"

#include "i2c.h"
#include "bsp_mpuxxxx_handler.h"


//******************************** Includes *********************************//


//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
void (*pf_pin_interrupt_callback)(void *, void *) = NULL;
void (*pf_dma_interrupt_callback)(void *, void *) = NULL;

/* CST816T touch INT (PB2 / TP_TINT_Pin) — owned by the touch hal_test task. */
void (*pf_cst816t_pin_interrupt_callback)(void *, void *) = NULL;
void  *g_cst816t_pin_interrupt_arg                        = NULL;

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
void HAL_GPIO_EXTI_Callback(UINT16_T GPIO_Pin)
{
    if (GPIO_PIN_5 == GPIO_Pin)
    {
        if (NULL != pf_pin_interrupt_callback)
        {
            pf_pin_interrupt_callback(mpuxxxx_handler_get_instance()->p_driver, NULL);
        }
    }
    else if (TP_TINT_Pin == GPIO_Pin)
    {
        if (NULL != pf_cst816t_pin_interrupt_callback)
        {
            pf_cst816t_pin_interrupt_callback(g_cst816t_pin_interrupt_arg,
                                              NULL);
        }
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c3)
    {
        pf_dma_interrupt_callback(mpuxxxx_handler_get_instance()->p_driver, NULL);
    }
}

/* USART1_IRQHandler moved to 02_MCU_Platform/MCU_Core_UART/uart_port/src/
   mcu_uart_port.c — vector handlers for managed peripherals live in their
   MCU port. HAL_UART_RxCpltCallback / HAL_UARTEx_RxEventCallback moved
   there too, so all USART1 ISR machinery is in one place.

   TODO(v5): mirror the same move for I2C — HAL_I2C_MemRxCpltCallback +
   HAL_GPIO_EXTI_Callback dispatch above should live in MCU_Core_IIC and
   a dedicated MCU_Core_GPIO port respectively, with per-bus / per-pin
   state tables exposing a clean register-callback API. Once done, this
   file can drop its `i2c.h` include and become purely "APP-owned sensor
   ISR fan-out" — no more CubeMX handle peek. */

//******************************* Functions *********************************//