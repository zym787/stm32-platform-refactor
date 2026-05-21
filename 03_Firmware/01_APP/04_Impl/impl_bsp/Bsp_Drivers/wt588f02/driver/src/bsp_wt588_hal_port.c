/******************************************************************************
 * @file bsp_wt588_hal_port.c
 *
 * @par dependencies
 * - bsp_wt588_hal_port.h
 * - gpio_port.h (MCU-port GPIO abstraction)
 * - tim.h
 * - osal_task.h
 *
 * @author Ethan-Hang
 *
 * @brief STM32 HAL concrete implementations for WT588 GPIO, busy-detect, and
 *        PWM-DMA interfaces.
 *
 * Processing flow:
 *
 * PWM-DMA protocol (WT588F02B one-wire serial, single-byte command):
 *   cmp_buff[0..6]  = 0     → 5.6 ms start-low (7 × 800 µs periods)
 *   cmp_buff[7..14] = CCR   → 8 data bits LSB-first
 *                             '1' → CCR = 600 (high 600 µs / low 200 µs)
 *                             '0' → CCR = 200 (high 200 µs / low 600 µs)
 *   cmp_buff[15]    = 800   → guard period (one full high period)
 *   TIM2 ARR = 800, PSC gives 1 µs tick → period = 800 µs
 *
 * Porting to a different MCU requires only replacing this file.
 *
 * @version V1.0 2026-04-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wt588_hal_port.h"
#include "gpio_port.h"
#include "tim.h"
#include "osal_task.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/** Timeout (ms) to wait for an ongoing DMA transfer to finish. */
#define WT588_DMA_SEND_TIMEOUT_MS   (200U)

/** DMA buffer length: 7 start bits + 8 data bits + 1 guard = 16 words. */
#define WT588_DMA_BUFF_LEN          (16U)

/** CCR value for data bit '1'. */
#define WT588_CCR_BIT1              (600U)

/** CCR value for data bit '0'. */
#define WT588_CCR_BIT0              (200U)

/** CCR value for the guard period (full high period). */
#define WT588_CCR_GUARD             (800U)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static wt588_status_t wt588_hal_gpio_init        (void);
static void           wt588_hal_gpio_deinit      (void);
static bool           wt588_hal_busy_is_busy     (void);
static wt588_status_t wt588_hal_pwm_dma_init     (void);
static void           wt588_hal_pwm_dma_deinit   (void);
static wt588_status_t wt588_hal_pwm_dma_send_byte(uint8_t data);
//******************************* Declaring *********************************//

//******************************* Variables *********************************//
/** DMA busy flag: set before HAL_DMA_Start_IT, cleared in DMA-complete ISR. */
static volatile bool s_tx_busy = false;

/** DMA CCR buffer — static so it remains valid during the async DMA transfer. */
static uint32_t s_cmp_buff[WT588_DMA_BUFF_LEN];
//******************************* Variables *********************************//

//******************************* Functions *********************************//

/* ========================================================================= */
/*  PWM-DMA ISR callback                                                      */
/* ========================================================================= */

/**
 * @brief DMA transfer-complete callback registered with HAL.
 *
 * Stops the TIM2 PWM output and clears the busy flag.
 */
static void wt588_dma_cplt_callback(DMA_HandleTypeDef *hdma)
{
    if (hdma == htim2.hdma[TIM_DMA_ID_UPDATE])
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        s_tx_busy = false;
    }
}

/* ========================================================================= */
/*  GPIO interface                                                             */
/* ========================================================================= */

/**
 * @brief GPIO init — TIM2_CH1 alternate function is already configured by
 *        MX_TIM2_Init() in STM32CubeMX-generated code, so nothing extra
 *        is required here.
 */
static wt588_status_t wt588_hal_gpio_init(void)
{
    return WT588_OK;
}

static void wt588_hal_gpio_deinit(void)
{
}

/* ========================================================================= */
/*  Busy interface                                                             */
/* ========================================================================= */

/**
 * @brief Report busy state by reading the hardware WT_BUSY pin.
 *        High level = busy (audio playing), low level = idle.
 */
static bool wt588_hal_busy_is_busy(void)
{
    core_gpio_pin_state_t state;
    AUDIO_GPIO_READ_BUSY(&state);
    return (CORE_GPIO_SET == state);
}

/* ========================================================================= */
/*  PWM-DMA interface                                                         */
/* ========================================================================= */

/**
 * @brief Initialise the PWM-DMA path.
 *
 * Registers the DMA-complete callback, enables the TIM2 update-event DMA
 * request, then waits 200 ms for the WT588F02B to complete its power-on
 * sequence.
 */
static wt588_status_t wt588_hal_pwm_dma_init(void)
{
    HAL_DMA_RegisterCallback(htim2.hdma[TIM_DMA_ID_UPDATE],
                             HAL_DMA_XFER_CPLT_CB_ID,
                             wt588_dma_cplt_callback);
    __HAL_TIM_ENABLE_DMA(&htim2, TIM_DMA_UPDATE);
    osal_task_delay(200U); /* WT588F02B power-on delay */
    return WT588_OK;
}

static void wt588_hal_pwm_dma_deinit(void)
{
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    __HAL_TIM_DISABLE_DMA(&htim2, TIM_DMA_UPDATE);
}

/**
 * @brief Send one command byte to the WT588F02B via TIM2 PWM + DMA.
 *
 * Blocks (via RTOS delay) until any previous transfer completes, then
 * builds the CCR buffer and starts a new non-blocking DMA transfer.
 *
 * @param data  Command byte (play index, stop code, or volume code).
 * @return WT588_OK on success, WT588_ERRORTIMEOUT if previous tx did not
 *         finish within WT588_DMA_SEND_TIMEOUT_MS.
 */
static wt588_status_t wt588_hal_pwm_dma_send_byte(uint8_t data)
{
    /* Wait for any previous transfer to finish */
    uint32_t timeout_ms = WT588_DMA_SEND_TIMEOUT_MS;
    while (s_tx_busy && (timeout_ms > 0U))
    {
        osal_task_delay(1U);
        timeout_ms--;
    }
    if (s_tx_busy)
    {
        return WT588_ERRORTIMEOUT;
    }

    /* Build CCR buffer:
     *   [0..6]  = 0       → 5.6 ms start-low (7 zero-duty periods)
     *   [7..14] = CCR     → 8 data bits, LSB first
     *   [15]    = GUARD   → one full high guard period
     */
    for (uint8_t i = 0U; i < 7U; i++)
    {
        s_cmp_buff[i] = 0U;
    }
    uint8_t d = data;
    for (uint8_t i = 7U; i < 15U; i++)
    {
        s_cmp_buff[i] = ((d & 0x01U) != 0U) ? WT588_CCR_BIT1 : WT588_CCR_BIT0;
        d >>= 1U;
    }
    s_cmp_buff[15] = WT588_CCR_GUARD;

    /* Start PWM and DMA */
    s_tx_busy = true;
    HAL_StatusTypeDef hal_ret;

    hal_ret = HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    DEBUG_OUT(d, WT588_HAL_PORT_LOG_TAG,
              "pwm_dma_send: PWM_Start ret=%d", (int)hal_ret);

    hal_ret = HAL_DMA_Start_IT(htim2.hdma[TIM_DMA_ID_UPDATE],
                               (uint32_t)s_cmp_buff,
                               (uint32_t)&htim2.Instance->CCR1,
                               WT588_DMA_BUFF_LEN);
    DEBUG_OUT(d, WT588_HAL_PORT_LOG_TAG,
              "pwm_dma_send: DMA_Start_IT ret=%d, DMA_State=%d",
              (int)hal_ret,
              (int)htim2.hdma[TIM_DMA_ID_UPDATE]->State);

    if (HAL_OK != hal_ret)
    {
        s_tx_busy = false;
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        return WT588_ERROR;
    }

    return WT588_OK;
}

/* ========================================================================= */
/*  Exported interface structs                                                 */
/* ========================================================================= */

wt_gpio_interface_t wt588_hal_gpio_interface = {
    .pf_wt_gpio_init   = wt588_hal_gpio_init,
    .pf_wt_gpio_deinit = wt588_hal_gpio_deinit,
};

wt_busy_interface_t wt588_hal_busy_interface = {
    .pf_is_busy = wt588_hal_busy_is_busy,
};

wt_pwm_dma_interface_t wt588_hal_pwm_dma_interface = {
    .pf_pwm_dma_init      = wt588_hal_pwm_dma_init,
    .pf_pwm_dma_deinit    = wt588_hal_pwm_dma_deinit,
    .pf_pwm_dma_send_byte = wt588_hal_pwm_dma_send_byte,
};

//******************************* Functions *********************************//
