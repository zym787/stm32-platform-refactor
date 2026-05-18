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

#include "i2c.h"
#include "usart.h"
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
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
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

/**
* @brief USART1 global interrupt vector handler. The HAL driver multiplexes
*        RXNE / IDLE / error events from inside HAL_UART_IRQHandler and
*        dispatches them to the registered HAL_UART_RxCpltCallback /
*        HAL_UARTEx_RxEventCallback (both live in 01_SERVICE/service_ota
*        and feed the OTA Ymodem listener + recv pipeline).
*
*        DMA2_Stream5 (UART1 RX DMA) IRQ stays in Core/Src/stm32f4xx_it.c
*        because CubeMX generates it from the .ioc when UART RX DMA is
*        enabled. Do NOT add a second DMA2_Stream5_IRQHandler here — the
*        linker would reject the duplicate symbol.
*
* @note  Lives in the App layer (not Core/Src/stm32f4xx_it.c) because it is
*        not a peripheral handler CubeMX owns — it's a manual addition
*        wiring the App-layer OTA path into the HAL.
* */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

//******************************* Functions *********************************//