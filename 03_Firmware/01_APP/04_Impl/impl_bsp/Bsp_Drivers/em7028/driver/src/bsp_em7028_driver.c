/******************************************************************************
 * @file       bsp_em7028_driver.c
 *
 * @par        dependencies
 * - bsp_em7028_driver.h
 * - bsp_em7028_reg.h
 * - Debug.h
 *
 * @author     Ethan-Hang
 *
 * @brief      EM7028 PPG heart-rate sensor driver implementation.
 *
 * Processing flow:
 * - bsp_em7028_driver_inst:
 *     1. Validate the four injected interfaces (iic, timebase, delay,
 *        os_delay) and reserve a PIMPL slot from the static pool.
 *     2. Mount default cached_cfg (40 Hz HRS1 pulse, HRS1 14-bit ADC).
 *     3. Mount the vtable.
 *     4. Auto-invoke pf_init via the freshly-mounted vtable. On failure
 *        the reserved PIMPL slot is rolled back so the next inst can
 *        reuse it.
 * - pf_init: bus init, boot delay, soft reset, ID probe, push the
 *   cached_cfg into hardware, mark is_init.
 * - pf_read_frame: burst-read enabled HRS data registers, pack a
 *   timestamped em7028_ppg_frame_t with per-channel pixel sums for the
 *   upper handler thread.
 *
 * @version    V1.0 2026-04-29
 *
 * @note       1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "bsp_em7028_driver.h"
#include "bsp_em7028_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Logging tags are defined centrally in Debug.h (EM7028_LOG_TAG /
 * EM7028_ERR_LOG_TAG) and routed to RTT Terminal 8 (DEBUG_RTT_CH_PPG). */

/* I2C transfer constants */
#define EM7028_I2C_TIMEOUT_MS                  100u
#define EM7028_I2C_MEM_SIZE_8BIT       0x00000001u

/* Power / reset timing */
#define EM7028_BOOT_DELAY_MS                     5u
#define EM7028_RESET_TOTAL_MS                    7u

/* HRSx data block layout */
#define EM7028_HRS_BLOCK_BYTES                   8u
#define EM7028_HRS2_BLOCK_BASE      HRS2_DATA0_L   /* 0x20                  */
#define EM7028_HRS1_BLOCK_BASE      HRS1_DATA0_L   /* 0x28                  */

/* Bit positions -- HRS_CFG (0x01) */
#define EM7028_HRS_CFG_HRS2_EN_BIT             (7u)
#define EM7028_HRS_CFG_HRS1_EN_BIT             (3u)

/* Bit positions -- HRS1_CTRL (0x0D)
 * Layout per Table 20:
 *   [7]   HRS_GAIN  : 0=x1, 1=x5
 *   [6]   HRS_RANGE : 0=x1, 1=x8
 *   [5:3] HRS_FREQ  : ADC clock / per-sample integration time
 *   [2:1] HRS_RES   : 00=10bit / 01=12bit / 10=14bit / 11=16bit
 *   [0]   IR_MODE   : 0=IR mode, 1=HRS1 mode (heart rate -- always 1) */
#define EM7028_HRS1_CTRL_GAIN_BIT              (7u)
#define EM7028_HRS1_CTRL_RANGE_BIT             (6u)
#define EM7028_HRS1_CTRL_FREQ_SHIFT            (3u)
#define EM7028_HRS1_CTRL_RES_SHIFT             (1u)
#define EM7028_HRS1_CTRL_IR_MODE_BIT           (0u)

/* Bit positions -- HRS2_CTRL (0x09)
 * Layout per Table 16:
 *   [7]   MODE      : 0=continuous, 1=pulse (gated by WAIT)
 *   [6:4] WAIT_TIME : sample period selector (em7028_hrs2_wait_t)
 *   [3:2] LED_WIDTH : LED pulse width  (driver-internal default)
 *   [1:0] LED_CNT   : LED pulse count  (driver-internal default)        */
#define EM7028_HRS2_CTRL_MODE_BIT              (7u)
#define EM7028_HRS2_CTRL_WAIT_SHIFT            (4u)

/* HRS2_CTRL low-bit driver defaults (not exposed via config struct).
 * LED_WIDTH = 0b01 -> 32 counts; LED_CNT = 0b01 -> 4 pulses.            */
#define EM7028_HRS2_CTRL_LED_WIDTH_DEFAULT  (0x01u << 2)
#define EM7028_HRS2_CTRL_LED_CNT_DEFAULT    (0x01u << 0)

/* Bit positions -- HRS2_GAIN_CTRL (0x0A)
 * Layout per Table 17:
 *   [7]   HRS2_GAIN : 0=x1, 1=x10
 *   [6:0] HRS2_POS  : per-pixel enable mask                             */
#define EM7028_HRS2_GAIN_CTRL_GAIN_BIT         (7u)
#define EM7028_HRS2_POS_MASK                (0x7Fu)

/* SOFT_RESET (0x0F) -- per Table 22 datasheet says ANY data write
 * triggers a chip reset; 0x4D is just our chosen canonical magic.       */
#define EM7028_SOFT_RESET_MAGIC             (0x4Du)

/* Consecutive I2C error warning threshold inside pf_read_frame */
#define EM7028_ERR_THRESHOLD                    5u

/* Maximum driver instances served from the static PIMPL pool */
#define EM7028_MAX_INSTANCES                    1u
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
struct em7028_private
{
    bool             is_init;
    em7028_config_t  cached_cfg;
    uint32_t         frame_seq;
    uint32_t         err_count;
};

static struct em7028_private gs_priv_pool[EM7028_MAX_INSTANCES];
static uint8_t               gs_priv_used = 0u;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief      Yield current task (prefer OS yield, fall back to busy delay).
 *
 * @param[in]  p_os_delay : Optional OS yield interface (may be NULL).
 *
 * @param[in]  p_delay    : Blocking delay interface (used when p_os_delay
 *                          is NULL or its function pointer is NULL).
 *
 * @param[in]  ms         : Wait duration in milliseconds.
 *
 * @param[out]            : None.
 *
 * @return     None.
 * */
static inline void em7028_yield_ms(
    em7028_os_delay_interface_t const * const p_os_delay,
    em7028_delay_interface_t      const * const p_delay,
    uint32_t                                    ms)
{
    /**
     * OS path is preferred when available so the thread does not monopolise
     * the CPU while the sensor finishes its internal reset sequence.
     **/
    if ((p_os_delay != NULL) && (p_os_delay->pf_rtos_delay != NULL))
    {
        p_os_delay->pf_rtos_delay(ms);
        return;
    }
    if ((p_delay != NULL) && (p_delay->pf_delay_ms != NULL))
    {
        p_delay->pf_delay_ms(ms);
    }
}

/**
 * @brief      Write bytes to a register via the mounted I2C interface.
 *
 * @param[in]  p_iic  : I2C interface bound to the driver instance.
 *
 * @param[in]  reg    : Target register address (8-bit).
 *
 * @param[in]  p_data : Payload buffer.
 *
 * @param[in]  len    : Payload length in bytes.
 *
 * @return     em7028_status_t I2C write status code.
 * */
static inline em7028_status_t em7028_write_reg(
    em7028_iic_driver_interface_t const * const p_iic,
    uint16_t                                    reg,
    uint8_t                                 * const p_data,
    uint16_t                                    len)
{
    /**
     * HAL mem APIs expect the 8-bit address; bit0 selects the R/W direction.
     **/
    return p_iic->pf_iic_mem_write(
        p_iic->hi2c,
        (EM7028_ADDR << 1) | 0,
        reg,
        EM7028_I2C_MEM_SIZE_8BIT,
        p_data,
        len,
        EM7028_I2C_TIMEOUT_MS);
}

/**
 * @brief      Read one or more consecutive registers via I2C.
 *
 * @param[in]  p_iic  : I2C interface bound to the driver instance.
 *
 * @param[in]  reg    : Starting register address (8-bit).
 *
 * @param[out] p_data : Buffer receiving the read payload.
 *
 * @param[in]  len    : Number of bytes to read.
 *
 * @return     em7028_status_t I2C read status code.
 * */
static inline em7028_status_t em7028_read_reg(
    em7028_iic_driver_interface_t const * const p_iic,
    uint16_t                                    reg,
    uint8_t                                 * const p_data,
    uint16_t                                    len)
{
    return p_iic->pf_iic_mem_read(
        p_iic->hi2c,
        (EM7028_ADDR << 1) | 1,
        reg,
        EM7028_I2C_MEM_SIZE_8BIT,
        p_data,
        len,
        EM7028_I2C_TIMEOUT_MS);
}

/**
 * @brief      Write a single byte to a register (convenience wrapper).
 *
 * @param[in]  p_iic : I2C interface bound to the driver instance.
 *
 * @param[in]  reg   : Target register address.
 *
 * @param[in]  value : Byte payload.
 *
 * @return     em7028_status_t I2C write status code.
 * */
static inline em7028_status_t em7028_write_byte(
    em7028_iic_driver_interface_t const * const p_iic,
    uint16_t                                    reg,
    uint8_t                                     value)
{
    uint8_t data = value;
    return em7028_write_reg(p_iic, reg, &data, 1);
}

/**
 * @brief      Validate that the I2C interface has all mandatory members.
 *
 * @param[in]  p_iic : I2C interface to validate.
 *
 * @return     true if all mandatory members are non-NULL, false otherwise.
 * */
static inline bool em7028_validate_iic(
    em7028_iic_driver_interface_t const * const p_iic)
{
    if ((p_iic == NULL)                          ||
        (p_iic->hi2c == NULL)                    ||
        (p_iic->pf_iic_init == NULL)             ||
        (p_iic->pf_iic_mem_read == NULL)         ||
        (p_iic->pf_iic_mem_write == NULL))
    {
        return false;
    }
    return true;
}

/**
 * @brief      Assemble HRS1_CTRL byte from config fields.
 *             IR_MODE is hard-coded to 1 (HRS1 / heart-rate mode); the
 *             driver never operates in IR mode.
 *
 * @param[in]  p_cfg : Validated configuration.
 *
 * @return     Assembled HRS1_CTRL register byte.
 * */
static inline uint8_t em7028_build_hrs1_ctrl(
    em7028_config_t const * const p_cfg)
{
    uint8_t val = 0;

    if (p_cfg->hrs1_gain_high)
    {
        val |= (1u << EM7028_HRS1_CTRL_GAIN_BIT);
    }
    if (p_cfg->hrs1_range_high)
    {
        val |= (1u << EM7028_HRS1_CTRL_RANGE_BIT);
    }
    val |= ((uint8_t)p_cfg->hrs1_freq << EM7028_HRS1_CTRL_FREQ_SHIFT);
    val |= ((uint8_t)p_cfg->hrs1_res  << EM7028_HRS1_CTRL_RES_SHIFT);

    /* IR_MODE = 1 selects HRS1 (heart-rate); IR mode is for proximity
     * sensing and is never used by this driver.                       */
    val |= (1u << EM7028_HRS1_CTRL_IR_MODE_BIT);
    return val;
}

/**
 * @brief      Assemble HRS2_CTRL byte from config fields.
 *             LED_WIDTH and LED_CNT are not exposed via config; we apply
 *             driver-internal defaults so the lower nibble is constant.
 *
 * @param[in]  p_cfg : Validated configuration.
 *
 * @return     Assembled HRS2_CTRL register byte.
 * */
static inline uint8_t em7028_build_hrs2_ctrl(
    em7028_config_t const * const p_cfg)
{
    uint8_t val = 0;

    if (p_cfg->hrs2_mode == EM7028_HRS2_MODE_PULSE)
    {
        val |= (1u << EM7028_HRS2_CTRL_MODE_BIT);
    }
    val |= ((uint8_t)p_cfg->hrs2_wait << EM7028_HRS2_CTRL_WAIT_SHIFT);
    val |= EM7028_HRS2_CTRL_LED_WIDTH_DEFAULT;
    val |= EM7028_HRS2_CTRL_LED_CNT_DEFAULT;
    return val;
}

/**
 * @brief      Assemble HRS2_GAIN_CTRL byte from config fields.
 *
 * @param[in]  p_cfg : Validated configuration.
 *
 * @return     Assembled HRS2_GAIN_CTRL register byte.
 * */
static inline uint8_t em7028_build_hrs2_gain(
    em7028_config_t const * const p_cfg)
{
    uint8_t val = 0;

    if (p_cfg->hrs2_gain_high)
    {
        val |= (1u << EM7028_HRS2_GAIN_CTRL_GAIN_BIT);
    }
    val |= (p_cfg->hrs2_pos_mask & EM7028_HRS2_POS_MASK);
    return val;
}

/**
 * @brief      Burst-read 8 bytes from consecutive HRS data registers and
 *             assemble them into 4 little-endian uint16_t pixel values.
 *
 * @param[in]  p_iic     : I2C interface bound to the driver instance.
 *
 * @param[in]  base_reg  : Starting data register (HRS1 or HRS2 base).
 *
 * @param[out] dst_pixel : Output array of 4 pixel values.
 *
 * @return     em7028_status_t I2C read status code.
 * */
static inline em7028_status_t em7028_burst_read_block(
    em7028_iic_driver_interface_t const * const p_iic,
    uint8_t                                     base_reg,
    uint16_t                          dst_pixel[EM7028_HRS_PIXEL_NUM])
{
    uint8_t buf[EM7028_HRS_BLOCK_BYTES];

    em7028_status_t ret = em7028_read_reg(p_iic, base_reg,
                                           buf, EM7028_HRS_BLOCK_BYTES);
    if (EM7028_OK != ret)
    {
        return ret;
    }

    /**
     * Byte order: _L (lower address) = LSB, _H (higher address) = MSB.
     * Reconstruct little-endian pairs into native uint16_t.
     **/
    dst_pixel[0] = (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    dst_pixel[1] = (uint16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    dst_pixel[2] = (uint16_t)(((uint16_t)buf[5] << 8) | buf[4]);
    dst_pixel[3] = (uint16_t)(((uint16_t)buf[7] << 8) | buf[6]);

    return EM7028_OK;
}

/**
 * @brief      Push the given config into hardware. Internal version --
 *             does NOT enforce is_init, so pf_init can call it before
 *             marking the driver initialised.
 *
 *             Sequence: clear HRS_CFG -> write LED_CRT -> HRS2_GAIN_CTRL
 *             -> HRS2_CTRL -> HRS1_CTRL -> cache the config. Enable bits
 *             of HRS_CFG are NOT set here -- pf_start opens the gate.
 *
 * @param[in]  self  : Driver instance pointer (assumed validated).
 *
 * @param[in]  p_cfg : Configuration to apply (assumed non-NULL).
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_apply_config_inner(
    struct bsp_em7028_driver *const self,
    em7028_config_t     const *const p_cfg)
{
    /* Step 1: stop sampling so register changes settle on a clean state */
    em7028_status_t ret = em7028_write_byte(self->p_iic, HRS_CFG, 0x00u);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write HRS_CFG register failed!");
        return ret;
    }

    /* Step 2: LED current driver setting */
    ret = em7028_write_byte(self->p_iic, LED_CRT, p_cfg->led_current);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write LED_CRT register failed!");
        return ret;
    }

    /* Step 3: HRS2 gain + pixel-position mask (register 0x0A) */
    ret = em7028_write_byte(self->p_iic, HRS2_GAIN_CTRL,
                             em7028_build_hrs2_gain(p_cfg));
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write HRS2_GAIN_CTRL register failed!");
        return ret;
    }

    /* Step 4: HRS2 control -- mode + wait + LED width/cnt defaults */
    ret = em7028_write_byte(self->p_iic, HRS2_CTRL,
                             em7028_build_hrs2_ctrl(p_cfg));
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write HRS2_CTRL register failed!");
        return ret;
    }

    /* Step 5: HRS1 control -- gain/range/freq/res, IR_MODE forced to 1 */
    ret = em7028_write_byte(self->p_iic, HRS1_CTRL,
                             em7028_build_hrs1_ctrl(p_cfg));
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write HRS1_CTRL register failed!");
        return ret;
    }

    /* Step 6: cache the config for start/stop/read_frame to reference */
    self->priv->cached_cfg = *p_cfg;
    return EM7028_OK;
}

/**
 * @brief      Issue a soft reset to the EM7028.
 *             ID verification is intentionally NOT done here -- pf_init
 *             owns the ID-probe step. This keeps soft_reset single-purpose.
 *
 * @param[in]  self : Driver instance pointer.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_soft_reset(
    struct bsp_em7028_driver *const self)
{
    if ((self == NULL) || (self->priv == NULL) || (self->p_iic == NULL))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 soft reset input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    /**
     * Per datasheet Table 22: writing any value to SOFT_RESET (0x0F)
     * triggers a chip reset.  EM7028_SOFT_RESET_MAGIC is just a
     * canonical sentinel for log readability.
     **/
    em7028_status_t ret = em7028_write_byte(self->p_iic, SOFT_RESET,
                                             EM7028_SOFT_RESET_MAGIC);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write SOFT_RESET register failed!");
        return ret;
    }

    em7028_yield_ms(self->p_os_delay, self->p_delay,
                     EM7028_RESET_TOTAL_MS);
    return EM7028_OK;
}

/**
 * @brief      Initialize the EM7028: bus init, boot delay, soft reset,
 *             ID probe, push cached_cfg into hardware. Marks priv->is_init
 *             true on success so subsequent gated APIs can run.
 *
 * @param[in]  self : Driver instance pointer.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_init(
    struct bsp_em7028_driver *const self)
{
    /* 1. validate external inputs */
    if ((self == NULL) || (self->priv == NULL))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 init input error parameter!");
        return EM7028_ERRORPARAMETER;
    }
    if (!em7028_validate_iic(self->p_iic))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 init iic interface missing ops!");
        return EM7028_ERRORPARAMETER;
    }

    /* 2. initialize I2C bus */
    em7028_status_t ret = self->p_iic->pf_iic_init(self->p_iic->hi2c);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG, "em7028 iic init failed!");
        return ret;
    }

    /**
     * EM7028 needs a short post-power-on settle window before the first
     * I2C transaction responds. 5 ms is a conservative default.
     **/
    em7028_yield_ms(self->p_os_delay, self->p_delay,
                     EM7028_BOOT_DELAY_MS);

    /* 3. soft reset to ensure clean state */
    ret = em7028_soft_reset(self);
    if (EM7028_OK != ret)
    {
        return ret;
    }

    /* 4. probe and verify chip id */
    uint8_t id = 0;
    ret = em7028_read_reg(self->p_iic, ID_REG, &id, 1);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read ID_REG register failed!");
        return ret;
    }
    if (EM7028_ID != id)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 id 0x%02X != 0x%02X", id, EM7028_ID);
        return EM7028_ERROR;
    }
    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 chip id ok: 0x%02X", id);

    /* 5. apply the cached configuration into hardware (inner variant
     * skips the is_init guard so this first apply succeeds)             */
    ret = em7028_apply_config_inner(self, &self->priv->cached_cfg);
    if (EM7028_OK != ret)
    {
        return ret;
    }

    self->priv->is_init   = true;
    self->priv->err_count = 0;
    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 init ok");
    return EM7028_OK;
}

/**
 * @brief      Deinitialize the driver: best-effort disable of both
 *             channels, release I2C transport, clear is_init.
 *             Idempotent and safe to call after a partial init.
 *
 * @param[in]  self : Driver instance pointer.
 *
 * @return     EM7028_OK on success; first I2C error encountered otherwise.
 * */
static em7028_status_t em7028_deinit(
    struct bsp_em7028_driver *const self)
{
    if ((self == NULL) || (self->priv == NULL))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 deinit input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    /**
     * Only attempt the chip-side disable when we actually completed init.
     * If init never finished, p_iic might be partially set up and writing
     * here would just generate noise.
     **/
    if (self->priv->is_init && (self->p_iic != NULL))
    {
        (void)em7028_write_byte(self->p_iic, HRS_CFG, 0x00u);
    }

    em7028_status_t ret = EM7028_OK;
    if ((self->p_iic != NULL) && (self->p_iic->pf_iic_deinit != NULL))
    {
        ret = self->p_iic->pf_iic_deinit(self->p_iic->hi2c);
    }

    self->priv->is_init = false;
    return ret;
}

/**
 * @brief      Read the device ID register (0x00). Caller compares it
 *             against EM7028_ID (0x36).
 *
 * @param[in]  self : Driver instance pointer.
 *
 * @param[out] p_id : Output ID byte.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_read_id(
    struct bsp_em7028_driver *const self,
    uint8_t                   *const p_id)
{
    if ((self == NULL) || (self->priv == NULL) ||
        (p_id == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read id input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    em7028_status_t ret = em7028_read_reg(self->p_iic, ID_REG, p_id, 1);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read ID_REG register failed!");
    }
    return ret;
}

/**
 * @brief      Apply a full config struct in one shot. is_init-gated;
 *             defers the actual register sequence to apply_config_inner
 *             so pf_init can share the same sequence pre-init.
 *
 * @param[in]  self  : Driver instance pointer.
 *
 * @param[in]  p_cfg : Configuration to apply.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_apply_config(
    struct bsp_em7028_driver *const self,
    em7028_config_t     const *const p_cfg)
{
    if ((self == NULL) || (self->priv == NULL) ||
        (p_cfg == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 apply config input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    return em7028_apply_config_inner(self, p_cfg);
}

/**
 * @brief      Start sampling -- set HRS_CFG enable bits per cached config.
 *
 * @param[in]  self : Driver instance pointer.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_start(
    struct bsp_em7028_driver *const self)
{
    if ((self == NULL) || (self->priv == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 start input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    uint8_t hrs_cfg = 0;
    em7028_status_t ret = em7028_read_reg(self->p_iic, HRS_CFG,
                                           &hrs_cfg, 1);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read HRS_CFG register failed!");
        return ret;
    }

    if (self->priv->cached_cfg.enable_hrs1)
    {
        hrs_cfg |= (1u << EM7028_HRS_CFG_HRS1_EN_BIT);
    }
    if (self->priv->cached_cfg.enable_hrs2)
    {
        hrs_cfg |= (1u << EM7028_HRS_CFG_HRS2_EN_BIT);
    }

    ret = em7028_write_byte(self->p_iic, HRS_CFG, hrs_cfg);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write HRS_CFG register failed!");
        return ret;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "em7028 start hrs1=%d hrs2=%d freq=%d wait=%d",
              self->priv->cached_cfg.enable_hrs1,
              self->priv->cached_cfg.enable_hrs2,
              self->priv->cached_cfg.hrs1_freq,
              self->priv->cached_cfg.hrs2_wait);
    return EM7028_OK;
}

/**
 * @brief      Stop sampling -- clear HRS_CFG enable bits.
 *             Cached config is preserved for a later pf_start.
 *
 * @param[in]  self : Driver instance pointer.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_stop(
    struct bsp_em7028_driver *const self)
{
    if ((self == NULL) || (self->priv == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 stop input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    uint8_t hrs_cfg = 0;
    em7028_status_t ret = em7028_read_reg(self->p_iic, HRS_CFG,
                                           &hrs_cfg, 1);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read HRS_CFG register failed!");
        return ret;
    }

    hrs_cfg &= ~((1u << EM7028_HRS_CFG_HRS1_EN_BIT) |
                  (1u << EM7028_HRS_CFG_HRS2_EN_BIT));

    ret = em7028_write_byte(self->p_iic, HRS_CFG, hrs_cfg);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write HRS_CFG register failed!");
        return ret;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 stop");
    return EM7028_OK;
}

/**
 * @brief      Burst-read enabled HRS data registers, assemble a PPG
 *             frame with timestamp and per-channel pixel sums.
 *             On any I2C failure the frame is NOT partially written.
 *
 * @param[in]  self    : Driver instance pointer.
 *
 * @param[out] p_frame : Output PPG frame.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_read_frame(
    struct bsp_em7028_driver *const self,
    em7028_ppg_frame_t       *const p_frame)
{
    if ((self == NULL) || (self->priv == NULL) ||
        (p_frame == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read frame input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    em7028_status_t ret = EM7028_OK;

    /* HRS2 channel */
    if (self->priv->cached_cfg.enable_hrs2)
    {
        ret = em7028_burst_read_block(self->p_iic,
                                       EM7028_HRS2_BLOCK_BASE,
                                       p_frame->hrs2_pixel);
        if (EM7028_OK != ret)
        {
            self->priv->err_count++;
            if (self->priv->err_count >= EM7028_ERR_THRESHOLD)
            {
                DEBUG_OUT(w, EM7028_ERR_LOG_TAG,
                          "em7028 consecutive err %lu",
                          (unsigned long)self->priv->err_count);
            }
            return ret;
        }
        p_frame->hrs2_sum = (uint32_t)p_frame->hrs2_pixel[0]
                           + (uint32_t)p_frame->hrs2_pixel[1]
                           + (uint32_t)p_frame->hrs2_pixel[2]
                           + (uint32_t)p_frame->hrs2_pixel[3];
    }
    else
    {
        memset(p_frame->hrs2_pixel, 0, sizeof(p_frame->hrs2_pixel));
        p_frame->hrs2_sum = 0;
    }

    /* HRS1 channel */
    if (self->priv->cached_cfg.enable_hrs1)
    {
        ret = em7028_burst_read_block(self->p_iic,
                                       EM7028_HRS1_BLOCK_BASE,
                                       p_frame->hrs1_pixel);
        if (EM7028_OK != ret)
        {
            self->priv->err_count++;
            if (self->priv->err_count >= EM7028_ERR_THRESHOLD)
            {
                DEBUG_OUT(w, EM7028_ERR_LOG_TAG,
                          "em7028 consecutive err %lu",
                          (unsigned long)self->priv->err_count);
            }
            return ret;
        }
        p_frame->hrs1_sum = (uint32_t)p_frame->hrs1_pixel[0]
                           + (uint32_t)p_frame->hrs1_pixel[1]
                           + (uint32_t)p_frame->hrs1_pixel[2]
                           + (uint32_t)p_frame->hrs1_pixel[3];
    }
    else
    {
        memset(p_frame->hrs1_pixel, 0, sizeof(p_frame->hrs1_pixel));
        p_frame->hrs1_sum = 0;
    }

    /* Stamp frame, advance counters, reset error streak */
    p_frame->timestamp_ms = self->p_timebase->pf_get_tick_count();
    self->priv->frame_seq++;
    self->priv->err_count = 0;

    return EM7028_OK;
}

/**
 * @brief      Raw register read -- escape hatch for thresholds, INT_CTRL,
 *             and other registers not covered by the typed API.
 *
 * @param[in]  self     : Driver instance pointer.
 *
 * @param[in]  reg_addr : Target register address.
 *
 * @param[out] p_val    : Output byte.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_read_reg_raw(
    struct bsp_em7028_driver *const self,
    uint8_t                     reg_addr,
    uint8_t                  *const p_val)
{
    if ((self == NULL) || (self->priv == NULL) ||
        (p_val == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read reg input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    em7028_status_t ret = em7028_read_reg(self->p_iic, reg_addr,
                                           p_val, 1);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 read reg 0x%02X failed!", reg_addr);
    }
    return ret;
}

/**
 * @brief      Raw register write -- escape hatch for thresholds, INT_CTRL,
 *             and other registers not covered by the typed API.
 *
 * @param[in]  self     : Driver instance pointer.
 *
 * @param[in]  reg_addr : Target register address.
 *
 * @param[in]  val      : Byte to write.
 *
 * @return     EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t em7028_write_reg_raw(
    struct bsp_em7028_driver *const self,
    uint8_t                     reg_addr,
    uint8_t                     val)
{
    if ((self == NULL) || (self->priv == NULL) || (!self->priv->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write reg input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    em7028_status_t ret = em7028_write_byte(self->p_iic, reg_addr, val);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 write reg 0x%02X failed!", reg_addr);
    }
    return ret;
}

/**
 * @brief      Instantiate the driver object: validate injected interfaces,
 *             reserve a PIMPL slot, mount defaults / interfaces / vtable,
 *             and auto-invoke pf_init via the freshly-mounted vtable.
 *             On any failure during init the reserved PIMPL slot is
 *             rolled back so the next call can reuse it.
 *
 * @param[in]  self       : Caller-allocated driver instance to populate.
 *
 * @param[in]  p_iic      : I2C transport interface (non-NULL).
 *
 * @param[in]  p_timebase : Monotonic ms tick interface (non-NULL).
 *
 * @param[in]  p_delay    : DWT hardware delay interface (non-NULL).
 *
 * @param[in]  p_os_delay : RTOS yield delay interface (may be NULL --
 *                          driver falls back to p_delay->pf_delay_ms).
 *
 * @param[out] self       : Populated driver instance, init complete.
 *
 * @return     EM7028_OK             on success;
 *             EM7028_ERRORPARAMETER for any NULL mandatory input;
 *             EM7028_ERRORNOMEMORY  when the private pool is exhausted;
 *             whatever pf_init returns on auto-init failure.
 * */
em7028_status_t bsp_em7028_driver_inst(
    bsp_em7028_driver_t                 *const         self,
    em7028_iic_driver_interface_t const *const        p_iic,
    em7028_timebase_interface_t   const *const   p_timebase,
    em7028_delay_interface_t      const *const      p_delay,
    em7028_os_delay_interface_t   const *const   p_os_delay)
{
    /* 1. validate mandatory pointer arguments */
    if ((self == NULL)        ||
        (p_iic == NULL)       ||
        (p_timebase == NULL)  ||
        (p_delay == NULL))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 inst input error parameter!");
        return EM7028_ERRORPARAMETER;
    }

    /* 2. validate mandatory I2C interface members */
    if (!em7028_validate_iic(p_iic))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 iic driver interface missing ops!");
        return EM7028_ERRORPARAMETER;
    }

    /* 3. validate timebase / delay providers */
    if (p_timebase->pf_get_tick_count == NULL)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 timebase interface missing ops!");
        return EM7028_ERRORPARAMETER;
    }
    if ((p_delay->pf_delay_ms == NULL) ||
        (p_delay->pf_delay_us == NULL))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 delay interface missing ops!");
        return EM7028_ERRORPARAMETER;
    }

    /* 4. allocate a PIMPL slot from the static pool (LIFO; rolled back
     *    on auto-init failure below)                                    */
    if (gs_priv_used >= EM7028_MAX_INSTANCES)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 private pool exhausted!");
        return EM7028_ERRORNOMEMORY;
    }
    self->priv = &gs_priv_pool[gs_priv_used];
    gs_priv_used++;
    memset(self->priv, 0, sizeof(*self->priv));

    /**
     * Project default: 40 Hz HRS1 in pulse mode, 14-bit ADC, gain x5,
     * range x8 (datasheet defaults except RES bumped to 14-bit for SNR
     * and FREQ moved from 100 ms to 25 ms to hit the 40 Hz target).
     * HRS2 is left disabled to save power -- enable via apply_config
     * if motion-artefact rejection is later added.
     **/
    self->priv->cached_cfg = (em7028_config_t){
        .enable_hrs1     = true,
        .enable_hrs2     = false,
        .hrs1_gain_high  = true,
        .hrs1_range_high = true,
        .hrs1_freq       = EM7028_HRS1_FREQ_163K_25MS,
        .hrs1_res        = EM7028_HRS_RES_14BIT,
        .hrs2_mode       = EM7028_HRS2_MODE_PULSE,
        .hrs2_wait       = EM7028_HRS2_WAIT_25MS,
        .hrs2_gain_high  = false,
        .hrs2_pos_mask   = 0x0Fu,
        .led_current     = 0x80u,
    };

    /* 5. mount external interfaces */
    self->p_iic      = p_iic;
    self->p_timebase = p_timebase;
    self->p_delay    = p_delay;
    self->p_os_delay = p_os_delay;

    /* 6. mount vtable */
    self->pf_init         = em7028_init;
    self->pf_deinit       = em7028_deinit;
    self->pf_soft_reset   = em7028_soft_reset;
    self->pf_read_id      = em7028_read_id;
    self->pf_apply_config = em7028_apply_config;
    self->pf_start        = em7028_start;
    self->pf_stop         = em7028_stop;
    self->pf_read_frame   = em7028_read_frame;
    self->pf_read_reg     = em7028_read_reg_raw;
    self->pf_write_reg    = em7028_write_reg_raw;
 
    /* 7. auto-invoke init via the freshly-mounted vtable */
    em7028_status_t ret = self->pf_init(self);
    if (EM7028_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 inst auto-init failed!");
        /* roll back the PIMPL slot so the caller can retry / next
         * instance can reuse it                                       */
        gs_priv_used--;
        self->priv = NULL;
        return ret;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 driver inst ok");
    return EM7028_OK;
}
//******************************* Functions *********************************//
