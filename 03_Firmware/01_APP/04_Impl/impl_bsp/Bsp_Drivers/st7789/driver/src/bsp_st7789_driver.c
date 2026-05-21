/******************************************************************************
 * @file    bsp_st7789_driver.c
 *
 * @par     dependencies
 * - bsp_st7789_driver.h
 * - Debug.h
 *
 * @author  Ethan-Hang
 *
 * @brief   ST7789 TFT-LCD controller driver implementation.
 *
 * Processing flow:
 * - Bind external interfaces via instantiation helper.
 * - Drive hardware reset, issue init command sequence, program addr window.
 * - Expose basic pixel / region / graphic / text primitives via vtable.
 *
 * @version V1.0 2026-04-24
 *
 * @note    1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_st7789_driver.h"
#include "bsp_st7789_reg.h"
#include "fonts.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define ST7789_IS_INITED                               true
#define ST7789_NOT_INITED                             false

/**
 * Sanity upper bound for the active window.  ST7789 controllers address a
 * 240 x 320 GRAM at most, so any panel larger than that is almost certainly
 * a caller mistake rather than a legitimate configuration.
 */
#define ST7789_MAX_PANEL_WIDTH                         240U
#define ST7789_MAX_PANEL_HEIGHT                        320U

/* Shortcuts to the driver-instance vtables.  Evaluated at call-site so the
 * macro argument must be a valid bsp_st7789_driver_t pointer. */
#define SPI_INSTANCE(driver_instance)      ((driver_instance)->p_spi_interface)
#define TIMEBASE_INSTANCE(driver_instance)                                    \
                                      ((driver_instance)->p_timebase_interface)
#define OS_INSTANCE(driver_instance)        ((driver_instance)->p_os_interface)

/* RESX / DCX / CSX logic level on the *_write_*_pin() helpers. */
#define ST7789_PIN_LOW                                   0U
#define ST7789_PIN_HIGH                                  1U

/**
 * Datasheet-mandated timings (ST7789T3 §7.4.5, §9.1.12).
 *   POWER_SETTLE: let VDD/VDDI settle before the first RESX edge.
 *   RESET_PULSE : TRW >= 10 us and < 5 ms; 1 ms is well within bounds.
 *   RESET_RECOVER: TRT = 120 ms, covers NVM load of ID / VCOM registers.
 *   SLPOUT      : spec requires >= 5 ms before next command; use 120 ms
 *                 so a later SLPIN also stays within spec without extra
 *                 bookkeeping.
 *   DISPON      : no spec minimum, but give DC/DC a moment before the
 *                 host enables backlight or starts pushing pixels.
 */
#define ST7789_DELAY_POWER_SETTLE_MS                    10U
#define ST7789_DELAY_RESET_PULSE_MS                      1U
#define ST7789_DELAY_RESET_RECOVER_MS                  120U
#define ST7789_DELAY_SLPOUT_MS                         120U
#define ST7789_DELAY_DISPON_MS                          10U

/**
 * Bulk-streaming primitives (fill_region / draw_image) share a single
 * RGB565 scratch tile.  256 pixels = 512 bytes strikes a balance between
 * SRAM footprint and DMA overhead: at 21 MHz SPI each tile takes ~200 us
 * to shift out, so a 240x240 full fill issues ~225 DMA transfers instead
 * of the ~57600 you would get at pixel granularity.
 */
#define ST7789_FILL_TILE_PIXELS                        256U
#define ST7789_FILL_TILE_BYTES              (ST7789_FILL_TILE_PIXELS * 2U)
#define ST7789_DMA_TIMEOUT_MS                          100U


#define ABS(x) (((x) < 0) ? -(x) : (x))
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
// static uint16_t s_disp_buf[ST7789_MAX_PANEL_WIDTH * ST7789_MAX_PANEL_HEIGHT];

/**
 * Pre-filled big-endian RGB565 scratch tile owned by fill_region.  Must
 * stay stable from the moment __st7789_write_data_dma queues a transfer
 * until pf_spi_wait_dma_complete() returns, since the DMA engine reads
 * directly from this buffer.
 */
static uint8_t s_fill_tile_buf[ST7789_FILL_TILE_BYTES];
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
st7789_status_t (__st7789_write_data       )(
                                   bsp_st7789_driver_t * const driver_instance,
                                               uint8_t   const *        p_data,
                                              uint32_t             data_length)
{
    st7789_status_t ret = ST7789_OK;
    if ((NULL == driver_instance) ||
        (NULL == p_data)          ||
        (0U   == data_length))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "write_data invalid argument");
        ret = ST7789_ERRORPARAMETER;
        return ret;
    }

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_LOW);
    SPI_INSTANCE(driver_instance)->pf_spi_write_dc_pin(ST7789_PIN_HIGH);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(p_data, data_length);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_HIGH);

    return ret;
}

/**
 * @brief  Transmit raw data bytes via DMA and block until the bus is idle.
 *
 * Owns the full DMA lifecycle (kick off + wait for completion) so callers
 * get the same "safe to touch the source buffer and release CS" semantics
 * they would from a blocking write, without writing the wait boilerplate
 * themselves.  The wait is mandatory anyway — sinking it here keeps the
 * primitive single-responsibility and rules out "forgot to wait" bugs.
 *
 * Caller contract:
 *   - Caller must have already asserted CS (LOW) and set DC=HIGH (data).
 *   - On return, the DMA has drained and CS may be released (or the next
 *     chunk queued) immediately.
 *
 * Meant for bulk-streaming paths (fill_region / draw_image) that wrap many
 * DMA chunks inside a single CS/DC envelope.  For one-shot transfers, use
 * __st7789_write_data() which owns its own CS envelope.
 * */
st7789_status_t (__st7789_write_data_dma)(
                                   bsp_st7789_driver_t * const driver_instance,
                                               uint8_t   const *        p_data,
                                              uint32_t             data_length)
{
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "write_data_dma null driver");
        return ST7789_ERRORPARAMETER;
    }

    st7789_status_t ret = SPI_INSTANCE(driver_instance)
                              ->pf_spi_transmit_dma(p_data, data_length);
    if (ST7789_OK != ret)
    {
        return ret;
    }

    return SPI_INSTANCE(driver_instance)
               ->pf_spi_wait_dma_complete(ST7789_DMA_TIMEOUT_MS);
}


st7789_status_t (__st7789_write_command    )(
                                   bsp_st7789_driver_t * const driver_instance,
                                               uint8_t                 command)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "write_data null driver");
        ret = ST7789_ERRORPARAMETER;
        return ret;
    }

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_LOW);
    SPI_INSTANCE(driver_instance)->pf_spi_write_dc_pin(ST7789_PIN_LOW);

    SPI_INSTANCE(driver_instance)->pf_spi_transmit(&command, 1U);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_HIGH);
    
    return ret;
}   



/**
 * @brief  Run power-on reset and ST7789 init command sequence.
 *
 *         Timings follow ST7789T3 datasheet §7.4.5 (Reset) and §9.1.12
 *         (SLPOUT).  Voltage / porch / gamma values are carried over
 *         from the Sitronix reference init that panel vendors ship as
 *         known-good on 240x240 IPS modules, so their bit patterns are
 *         intentionally opaque here.
 *
 * @param[in] driver_instance : Driver object already populated by
 *                              bsp_st7789_driver_init().
 *
 * @return ST7789_OK on success, ST7789_ERRORPARAMETER on NULL driver.
 * */
static st7789_status_t st7789_init(bsp_st7789_driver_t *const driver_instance)
{
    /**
     * Input parameter check.
     * st7789_init is invoked before the RTOS scheduler starts (from
     * User_Init), so we cannot rely on osal-level fault handling and
     * must validate the driver object + its mounted interfaces here.
     **/
    if ((NULL == driver_instance)                                            ||
        (NULL == driver_instance->p_spi_interface)                           ||
        (NULL == driver_instance->p_timebase_interface))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "st7789_init input error parameter");
        return ST7789_ERRORPARAMETER;
    }

    DEBUG_OUT(i, ST7789_LOG_TAG, "st7789_init: hw reset");

    /**
     * ===== Phase 1 : Hardware reset (Table 9) =====
     * RESX high → settle supplies → RESX low (>= 10us, <= 5ms pulse) →
     * RESX high → wait 120 ms for NVM → register load.  Busy-wait is
     * used because this runs before the FreeRTOS scheduler starts, so
     * pf_os_delay_ms (vTaskDelay) would fault.
     **/
    SPI_INSTANCE(driver_instance)->pf_spi_write_rst_pin(ST7789_PIN_HIGH);
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(
                                            ST7789_DELAY_POWER_SETTLE_MS);
    SPI_INSTANCE(driver_instance)->pf_spi_write_rst_pin(ST7789_PIN_LOW);
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(
                                            ST7789_DELAY_RESET_PULSE_MS);
    SPI_INSTANCE(driver_instance)->pf_spi_write_rst_pin(ST7789_PIN_HIGH);
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(
                                            ST7789_DELAY_RESET_RECOVER_MS);

    /**
     * ===== Phase 2 : Leave sleep (§9.1.12) =====
     * SLPOUT starts DC/DC, oscillator and panel scanning.  Datasheet
     * mandates >= 5 ms before the next command; 120 ms also covers the
     * SLPIN-after-SLPOUT interval so later code can issue sleep in
     * without extra bookkeeping.
     **/
    __st7789_write_command(driver_instance, ST7789_SLPOUT);
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(ST7789_DELAY_SLPOUT_MS);

    /**
     * ===== Phase 3 : Pixel format + orientation =====
     * 16-bit RGB565 matches the driver's uint16_t color API.  MADCTL
     * default keeps the panel's native orientation; rotation support
     * will be added via a dedicated helper later.
     **/
    __st7789_write_command(driver_instance, ST7789_COLMOD);
    __st7789_write_data(driver_instance, (uint8_t[1]){ST7789_COLOR_MODE_16bit}, 1U);
    __st7789_write_command(driver_instance, ST7789_MADCTL);
    __st7789_write_data(driver_instance, (uint8_t[1]){ST7789_MADCTL_RGB}, 1U);

    /**
     * ===== Phase 4 : Panel-specific timing / voltage tuning =====
     * Values are the Sitronix reference / panel-vendor tuned set used
     * on common 240x240 IPS modules.  Keep as-is unless the specific
     * panel datasheet overrides them.
     **/
    {
        /* PORCTRL (0xB2) : porch setting, 5 bytes. */
        static const uint8_t porctrl[] = {
            0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U
        };
        __st7789_write_command(driver_instance, ST7789_PORCTRL);
        __st7789_write_data(driver_instance, porctrl, sizeof(porctrl));
    }

    /* GCTRL (0xB7) = 0x35 : VGH = 13.26V, VGL = -10.43V. */
    __st7789_write_command(driver_instance, ST7789_GCTRL);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x35U}, 1U);

    /* VCOMS (0xBB) = 0x19 : VCOM = 0.725V (panel-tuned). */
    __st7789_write_command(driver_instance, ST7789_VCOMS);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x19U}, 1U);

    /* LCMCTRL (0xC0) = 0x2C : LCM control default. */
    __st7789_write_command(driver_instance, ST7789_LCMCTRL);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x2CU}, 1U);

    /* VDVVRHEN (0xC2) = 0x01 : enable VDV / VRH control via command set. */
    __st7789_write_command(driver_instance, ST7789_VDVVRHEN);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x01U}, 1U);

    /* VRHS (0xC3) = 0x12 : VRH = +-4.45V. */
    __st7789_write_command(driver_instance, ST7789_VRHS);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x12U}, 1U);

    /* VDVS (0xC4) = 0x20 : VDV default. */
    __st7789_write_command(driver_instance, ST7789_VDVS);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x20U}, 1U);

    /* FRCTRL2 (0xC6) = 0x0F : normal-mode frame rate 60 Hz. */
    __st7789_write_command(driver_instance, ST7789_FRCTRL2);
    __st7789_write_data(driver_instance, (uint8_t[1]){0x0FU}, 1U);

    {
        /* PWCTRL1 (0xD0) : AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V, 2 bytes. */
        static const uint8_t pwctrl1[] = { 0xA4U, 0xA1U };
        __st7789_write_command(driver_instance, ST7789_PWCTRL1);
        __st7789_write_data(driver_instance, pwctrl1, sizeof(pwctrl1));
    }

    {
        /* PVGAMCTRL (0xE0) : positive gamma curve, 14 bytes (panel-tuned). */
        static const uint8_t pvgamctrl[] = {
            0xD0U, 0x04U, 0x0DU, 0x11U, 0x13U, 0x2BU, 0x3FU,
            0x54U, 0x4CU, 0x18U, 0x0DU, 0x0BU, 0x1FU, 0x23U
        };
        __st7789_write_command(driver_instance, ST7789_PVGAMCTRL);
        __st7789_write_data(driver_instance, pvgamctrl, sizeof(pvgamctrl));
    }

    {
        /* NVGAMCTRL (0xE1) : negative gamma curve, 14 bytes (panel-tuned). */
        static const uint8_t nvgamctrl[] = {
            0xD0U, 0x04U, 0x0CU, 0x11U, 0x13U, 0x2CU, 0x3FU,
            0x44U, 0x51U, 0x2FU, 0x1FU, 0x1FU, 0x20U, 0x23U
        };
        __st7789_write_command(driver_instance, ST7789_NVGAMCTRL);
        __st7789_write_data(driver_instance, nvgamctrl, sizeof(nvgamctrl));
    }

    /**
     * ===== Phase 5 : Colour polarity + display mode =====
     * IPS ST7789 modules need INVON (native polarity is inverted);
     * NORON is redundant after H/W reset but kept for explicit intent.
     **/
    __st7789_write_command(driver_instance, ST7789_INVON);
    __st7789_write_command(driver_instance, ST7789_NORON);

    /**
     * ===== Phase 6 : Turn display on =====
     * H/W reset does not clear frame memory, so a follow-up fill_color
     * call (black) is expected to hide the initial RAM contents.  That
     * is left to the caller since fill_color is not yet implemented.
     **/
    __st7789_write_command(driver_instance, ST7789_DISPON);
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(ST7789_DELAY_DISPON_MS);

    driver_instance->is_init = ST7789_IS_INITED;

    DEBUG_OUT(d, ST7789_LOG_TAG, "st7789_init complete");
    return ST7789_OK;
}

/**
 * @brief  Release hardware resources and mark the instance uninitialized.
 *
 * @param[in] driver_instance : Driver object.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_deinit(bsp_st7789_driver_t *const driver_instance)
{
    /* TODO: DISPOFF + SLPIN + SPI deinit. */
    (void)driver_instance;
    return ST7789_OK;
}

/**
 * @brief  Program CASET / RASET so subsequent RAMWR writes land in the
 *         given inclusive rectangle.  This is the foundation of every
 *         L3 drawing primitive: once the window is open, the caller
 *         simply streams RGB565 data to the panel via write_data().
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x_start, y_start: Top-left corner (inclusive, panel coords).
 * @param[in] x_end,   y_end  : Bottom-right corner (inclusive, panel coords).
 *
 * @return ST7789_OK on success,
 *         ST7789_ERRORPARAMETER on NULL driver or out-of-panel window.
 * */
static st7789_status_t __st7789_set_addr_window(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                x_start,
                                               uint16_t                y_start,
                                               uint16_t                  x_end,
                                               uint16_t                  y_end)
{
    /* Guard against mis-dispatch from the vtable. */
    if (NULL == driver_instance)
    {
        return ST7789_ERRORPARAMETER;
    }

    /**
     * Reject inverted or out-of-panel rectangles here once so every
     * L3 primitive (draw_pixel / fill_region / draw_image) can assume
     * its window is valid without re-clipping.
     **/
    if ((x_start  > x_end)                                              ||
        (y_start  > y_end)                                              ||
        (x_end   >= driver_instance->panel.width)                       ||
        (y_end   >= driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "set_addr_window out of range");
        return ST7789_ERRORPARAMETER;
    }

    /**
     * Translate to controller GRAM coordinates.  Some ST7789 modules
     * place the active pixels in a sub-window of the 240x320 GRAM
     * (e.g. 135x240 and 240x240 modules start at non-zero Y), so every
     * CASET / RASET has to add panel.{x,y}_offset.
     **/
    const uint16_t xs = x_start + driver_instance->panel.x_offset;
    const uint16_t xe = x_end   + driver_instance->panel.x_offset;
    const uint16_t ys = y_start + driver_instance->panel.y_offset;
    const uint16_t ye = y_end   + driver_instance->panel.y_offset;

    /* CASET (0x2A) : column range, big-endian 16-bit pairs. */
    uint8_t caset[4];
    caset[0] = (uint8_t)(xs >> 8);
    caset[1] = (uint8_t)(xs & 0xFFU);
    caset[2] = (uint8_t)(xe >> 8);
    caset[3] = (uint8_t)(xe & 0xFFU);
    __st7789_write_command(driver_instance,
                           ST7789_CASET);
    __st7789_write_data(driver_instance, 
                        caset, 
                        sizeof(caset));

    /* RASET (0x2B) : row range, big-endian 16-bit pairs. */
    uint8_t raset[4];
    raset[0] = (uint8_t)(ys >> 8);
    raset[1] = (uint8_t)(ys & 0xFFU);
    raset[2] = (uint8_t)(ye >> 8);
    raset[3] = (uint8_t)(ye & 0xFFU);
    __st7789_write_command(driver_instance, ST7789_RASET);
    __st7789_write_data(driver_instance, raset, sizeof(raset));

    /**
     * RAMWR (0x2C) opens the write transaction.  Bulk pixel data is
     * the caller's responsibility; pass data_length = 0 so the adapter
     * does not assume an immediate follow-up within this call.
     **/
    __st7789_write_command(driver_instance, ST7789_RAMWR);
    return ST7789_OK;
}

/**
 * @brief  Draw a single pixel at (x, y).  Opens a 1x1 addr window via
 *         __st7789_set_addr_window, then streams two RGB565 bytes.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x               : Column in panel coordinates.
 * @param[in] y               : Row    in panel coordinates.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success,
 *         ST7789_ERRORPARAMETER on NULL driver or off-panel coordinate.
 * */
static st7789_status_t st7789_draw_pixel(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                      x,
                                               uint16_t                      y,
                                               uint16_t                  color)
{
    if (NULL == driver_instance)
    {
        return ST7789_ERRORPARAMETER;
    }

    /**
     * Delegate bounds checking to set_addr_window.  Call the static
     * helper directly instead of through the vtable: it avoids a level
     * of indirection and stays correct even if a caller swaps the
     * vtable slot out mid-flight (e.g. during rotation change).
     **/
    st7789_status_t ret = __st7789_set_addr_window(driver_instance, x, y, x, y);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_pixel invalid coordinate (%u, %u)", x, y);
        return ret;
    }

    /* RGB565 is sent MSB-first on SPI: high byte, then low byte. */
    static uint8_t pixel[2];
    memset(pixel, 0, sizeof(pixel));
    pixel[0] = (uint8_t)(color >> 8);
    pixel[1] = (uint8_t)(color & 0xFFU);
    ret = __st7789_write_data(driver_instance, pixel, sizeof(pixel));
    return ret;
}

/**
 * @brief  Draw a 2x2 pixel block at (x, y) for thick pixel / low-res UI.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x, y            : Top-left pixel of the 2x2 block.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_pixel_4px(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                      x,
                                               uint16_t                      y,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_pixel_4px null driver");
        ret = ST7789_ERRORPARAMETER;
        return ret;
    }

    ret = __st7789_set_addr_window(driver_instance, x, y, x + 1U, y + 1U);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_pixel_4px invalid coordinate (%u, %u)", x, y);
        return ret;
    }

    static uint8_t pixel[8];
    memset(pixel, 0, sizeof(pixel));
    for (uint8_t i = 0; i < 4U; i++)
    {
        pixel[2U * i]     = (uint8_t)(color >> 8);
        pixel[2U * i + 1] = (uint8_t)(color & 0xFFU);
    }
    ret = __st7789_write_data(driver_instance, pixel, sizeof(pixel));
    return ret;
}

/**
 * @brief  Fill an inclusive rectangular region with a single color.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x_start, y_start: Top-left corner (inclusive).
 * @param[in] x_end,   y_end  : Bottom-right corner (inclusive).
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_fill_region(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                x_start,
                                               uint16_t                y_start,
                                               uint16_t                  x_end,
                                               uint16_t                  y_end,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        ret = ST7789_ERRORPARAMETER;
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "fill_region null driver");
        return ret;
    }

    ret = __st7789_set_addr_window(driver_instance, x_start, y_start, x_end, y_end);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "fill_region invalid coordinate (%u, %u) to (%u, %u)",
                  x_start, y_start, x_end, y_end);
        return ret;
    }

    /**
     * Pre-fill the scratch tile with the target color (big-endian RGB565).
     * Re-filling every call keeps the code stateless; the 512-byte memset
     * is negligible next to the DMA transfers that follow.
     **/
    const uint8_t hi = (uint8_t)(color >> 8);
    const uint8_t lo = (uint8_t)(color & 0xFFU);
    for (uint32_t i = 0U; i < ST7789_FILL_TILE_BYTES; i += 2U)
    {
        s_fill_tile_buf[i]      = hi;
        s_fill_tile_buf[i + 1U] = lo;
    }

    /**
     * Stream the tile in CS-wide chunks.  set_addr_window left the panel
     * mid-RAMWR with CS released; re-asserting CS here keeps the entire
     * fill inside one SPI transaction so the controller treats it as a
     * continuous GRAM burst.  DC stays HIGH throughout — we never issue a
     * command during the fill.
     **/
    const uint32_t total_px = (uint32_t)(x_end - x_start + 1U) *
                              (uint32_t)(y_end - y_start + 1U);
    uint32_t remain_px = total_px;

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_LOW);
    SPI_INSTANCE(driver_instance)->pf_spi_write_dc_pin(ST7789_PIN_HIGH);

    while (remain_px > 0U)
    {
        const uint32_t chunk_px = (remain_px > ST7789_FILL_TILE_PIXELS)
                                  ? ST7789_FILL_TILE_PIXELS : remain_px;

        ret = __st7789_write_data_dma(driver_instance,
                                      s_fill_tile_buf,
                                      chunk_px * 2U);
        if (ST7789_OK != ret)
        {
            DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                      "fill_region dma chunk failed = %d", (int)ret);
            break;
        }

        remain_px -= chunk_px;
    }

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_HIGH);
    return ret;
}

/**
 * @brief  Fill the entire active panel area with a single RGB565 color.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_fill_color(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        ret = ST7789_ERRORPARAMETER;
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "fill_color null driver");
        return ret;
    }

    ret = st7789_fill_region(driver_instance, 0U, 0U,
                             driver_instance->panel.width - 1U,
                             driver_instance->panel.height - 1U,
                             color);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "fill_color failed = %d", (int)ret);
    }

    return ret;
}

/**
 * @brief  Blit a caller-supplied RGB565 bitmap to the given region.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x_start, y_start: Top-left paste coordinate.
 * @param[in] w, h            : Bitmap width / height in pixels.
 * @param[in] bitmap          : Pointer to w*h RGB565 pixels (row-major).
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_image(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                x_start,
                                               uint16_t                y_start,
                                               uint16_t                      w,
                                               uint16_t                      h,
                                               uint16_t  const*         bitmap)
{
    st7789_status_t ret = ST7789_OK;
    if ((NULL == driver_instance) ||
        (NULL == bitmap)         ||
        (0U   == w)              ||
        (0U   == h))
    {
        ret = ST7789_ERRORPARAMETER;
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_image invalid argument");
        return ret;
    }

    if ((x_start >= driver_instance->panel.width)                         ||
        (y_start >= driver_instance->panel.height)                        ||
        ((uint32_t)x_start + (uint32_t)w > (uint32_t)driver_instance->panel.width) ||
        ((uint32_t)y_start + (uint32_t)h > (uint32_t)driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_image out of range (%u,%u) w=%u h=%u",
                  x_start, y_start, w, h);
        return ST7789_ERRORPARAMETER;
    }

    ret = __st7789_set_addr_window(driver_instance, 
                                   x_start, y_start,
                                   x_start + w - 1U, y_start + h - 1U);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_image set window failed = %d", (int)ret);
        return ret;
    }

    {
        const uint32_t total_px = (uint32_t)w * (uint32_t)h;
        uint32_t pixel_offset = 0U;

        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_LOW);
        SPI_INSTANCE(driver_instance)->pf_spi_write_dc_pin(ST7789_PIN_HIGH);

        while (pixel_offset < total_px)
        {
            const uint32_t chunk_px =
                ((total_px - pixel_offset) > ST7789_FILL_TILE_PIXELS)
                    ? ST7789_FILL_TILE_PIXELS
                    : (total_px - pixel_offset);

            for (uint32_t idx = 0U; idx < chunk_px; idx++)
            {
                const uint16_t pixel = bitmap[pixel_offset + idx];
                s_fill_tile_buf[2U * idx] = (uint8_t)(pixel >> 8);
                s_fill_tile_buf[(2U * idx) + 1U] = (uint8_t)(pixel & 0xFFU);
            }

            ret = __st7789_write_data_dma(driver_instance,
                                          s_fill_tile_buf,
                                          chunk_px * 2U);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                          "draw_image dma chunk failed = %d", (int)ret);
                break;
            }

            pixel_offset += chunk_px;
        }

        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_HIGH);
    }

    return ret;
}

/**
 * @brief  Draw a straight line between two endpoints.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x0, y0          : First endpoint.
 * @param[in] x1, y1          : Second endpoint.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_line(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                     x0,
                                               uint16_t                     y0,
                                               uint16_t                     x1,
                                               uint16_t                     y1,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        ret = ST7789_ERRORPARAMETER;
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_line null driver");
        return ret;
    }

    if (x0 == x1 || y0 == y1)
    {
        const uint16_t x_start = (x0 < x1) ? x0 : x1;
        const uint16_t y_start = (y0 < y1) ? y0 : y1;
        const uint16_t x_end   = (x0 < x1) ? x1 : x0;
        const uint16_t y_end   = (y0 < y1) ? y1 : y0;

        ret = st7789_fill_region(driver_instance,
                                 x_start, y_start,
                                 x_end, y_end,
                                 color);
        if (ST7789_OK != ret)
        {
            DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                    "draw_line fill_region failed = %d", (int)ret);
        }
    }
    else
    {
        int16_t x = (int16_t)x0;
        int16_t y = (int16_t)y0;
        const int16_t x_end = (int16_t)x1;
        const int16_t y_end = (int16_t)y1;

        const int16_t dx = (int16_t)ABS(x_end - x);
        const int16_t dy = (int16_t)ABS(y_end - y);
        const int16_t sx = (x < x_end) ? 1 : -1;
        const int16_t sy = (y < y_end) ? 1 : -1;

        int16_t err = (int16_t)(dx - dy);

        /* Integer Bresenham: advance x/y based on accumulated error. */
        while (1)
        {
            ret = st7789_draw_pixel(driver_instance,
                                    (uint16_t)x,
                                    (uint16_t)y,
                                    color);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                          "draw_line draw_pixel failed = %d", (int)ret);
                break;
            }

            if ((x == x_end) && (y == y_end))
            {
                break;
            }

            {
                const int16_t e2 = (int16_t)(err + err);

                if (e2 > -dy)
                {
                    err = (int16_t)(err - dy);
                    x = (int16_t)(x + sx);
                }

                if (e2 < dx)
                {
                    err = (int16_t)(err + dx);
                    y = (int16_t)(y + sy);
                }
            }
        }
    }

    return ret;
}

/**
 * @brief  Draw a hollow rectangle outline.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x0, y0          : Top-left corner.
 * @param[in] x1, y1          : Bottom-right corner.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_rectangle(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                     x0,
                                               uint16_t                     y0,
                                               uint16_t                     x1,
                                               uint16_t                     y1,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        ret = ST7789_ERRORPARAMETER;
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_rectangle null driver");
        return ret;
    }

    /* Four lines: top, bottom, left, right. */
    ret = st7789_draw_line(driver_instance, x0, y0, x1, y0, color);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_rectangle top line failed = %d", (int)ret);
        return ret;
    }
    ret = st7789_draw_line(driver_instance, x0, y1, x1, y1, color);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_rectangle bottom line failed = %d", (int)ret);
        return ret;
    }
    ret = st7789_draw_line(driver_instance, x0, y0, x0, y1, color);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_rectangle left line failed = %d", (int)ret);
        return ret;
    }
    ret = st7789_draw_line(driver_instance, x1, y0, x1, y1, color);
    if (ST7789_OK != ret)    
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                    "draw_rectangle right line failed = %d", (int)ret);
        return ret;
    }

    return ret;
}

/**
 * @brief  Draw a hollow circle centered at (x_center, y_center).
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x_center, y_center: Circle center.
 * @param[in] radius          : Radius in pixels.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_circle(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t               x_center,
                                               uint16_t               y_center,
                                               uint16_t                 radius,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_circle null driver");
        return ST7789_ERRORPARAMETER;
    }

    if ((x_center >= driver_instance->panel.width) ||
        (y_center >= driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_circle center out of range (%u, %u)",
                  x_center, y_center);
        return ST7789_ERRORPARAMETER;
    }

    {
        const int32_t x_min = (int32_t)x_center - (int32_t)radius;
        const int32_t y_min = (int32_t)y_center - (int32_t)radius;
        const int32_t x_max = (int32_t)x_center + (int32_t)radius;
        const int32_t y_max = (int32_t)y_center + (int32_t)radius;

        if ((x_min < 0) ||
            (y_min < 0) ||
            (x_max >= (int32_t)driver_instance->panel.width) ||
            (y_max >= (int32_t)driver_instance->panel.height))
        {
            DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                      "draw_circle radius out of range r=%u", radius);
            return ST7789_ERRORPARAMETER;
        }
    }

    if (0U == radius)
    {
        return st7789_draw_pixel(driver_instance, x_center, y_center, color);
    }

    {
        int32_t x = (int32_t)radius;
        int32_t y = 0;
        int32_t err = 1 - x;

        while (x >= y)
        {
            const int32_t px[8] = {
                (int32_t)x_center + x, (int32_t)x_center - x,
                (int32_t)x_center + x, (int32_t)x_center - x,
                (int32_t)x_center + y, (int32_t)x_center - y,
                (int32_t)x_center + y, (int32_t)x_center - y,
            };
            const int32_t py[8] = {
                (int32_t)y_center + y, (int32_t)y_center + y,
                (int32_t)y_center - y, (int32_t)y_center - y,
                (int32_t)y_center + x, (int32_t)y_center + x,
                (int32_t)y_center - x, (int32_t)y_center - x,
            };

            for (uint8_t idx = 0U; idx < 8U; idx++)
            {
                ret = st7789_draw_pixel(driver_instance,
                                        (uint16_t)px[idx],
                                        (uint16_t)py[idx],
                                        color);
                if (ST7789_OK != ret)
                {
                    DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                              "draw_circle draw_pixel failed = %d",
                              (int)ret);
                    return ret;
                }
            }

            y++;
            if (err < 0)
            {
                err += (2 * y) + 1;
            }
            else
            {
                x--;
                err += (2 * (y - x)) + 1;
            }
        }
    }

    return ret;
}

/**
 * @brief  Enable / disable display color inversion (INVON / INVOFF).
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] invert          : Non-zero enables inversion.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t invert_colors(
                                    bsp_st7789_driver_t *const driver_instance,
                                                uint8_t                 invert)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "invert_colors null driver");
        return ST7789_ERRORPARAMETER;
    }

    ret = __st7789_write_command(driver_instance,
                                 (0U != invert) ? ST7789_INVON : ST7789_INVOFF);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "invert_colors failed = %d", (int)ret);
    }

    return ret;
}

/**
 * @brief  Draw a single ASCII character using the embedded glyph table.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x, y            : Top-left glyph cell.
 * @param[in] ch              : ASCII code point.
 * @param[in] color, bg_color : Foreground / background RGB565.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_char(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                      x,
                                               uint16_t                      y,
                                                   char                     ch,
                                               uint16_t                  color,
                                               uint16_t               bg_color)
{
    st7789_status_t ret = ST7789_OK;
    const font_def_t *font = &Font_7x10_t;
    uint8_t char_code = (uint8_t)ch;

    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_char null driver");
        return ST7789_ERRORPARAMETER;
    }

    if ((char_code < 32U) || (char_code > 126U))
    {
        char_code = (uint8_t)'?';
    }

    if ((x >= driver_instance->panel.width)                                ||
        (y >= driver_instance->panel.height)                               ||
        ((uint32_t)x + (uint32_t)font->width >
         (uint32_t)driver_instance->panel.width)                           ||
        ((uint32_t)y + (uint32_t)font->height >
         (uint32_t)driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_char out of range (%u,%u)", x, y);
        return ST7789_ERRORPARAMETER;
    }

    ret = __st7789_set_addr_window(driver_instance,
                                   x,
                                   y,
                                   (uint16_t)(x + font->width - 1U),
                                   (uint16_t)(y + font->height - 1U));
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_char set window failed = %d", (int)ret);
        return ret;
    }

    {
        const uint32_t glyph_base =
            (uint32_t)(char_code - 32U) * (uint32_t)font->height;
        uint32_t tile_pixels = 0U;

        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_LOW);
        SPI_INSTANCE(driver_instance)->pf_spi_write_dc_pin(ST7789_PIN_HIGH);

        for (uint32_t row = 0U; row < (uint32_t)font->height; row++)
        {
            const uint16_t glyph_row = font->data[glyph_base + row];

            for (uint32_t col = 0U; col < (uint32_t)font->width; col++)
            {
                const uint16_t pixel = (((glyph_row << col) & 0x8000U) != 0U)
                                           ? color
                                           : bg_color;

                s_fill_tile_buf[tile_pixels * 2U] = (uint8_t)(pixel >> 8);
                s_fill_tile_buf[(tile_pixels * 2U) + 1U] =
                    (uint8_t)(pixel & 0xFFU);

                tile_pixels++;

                if (ST7789_FILL_TILE_PIXELS == tile_pixels)
                {
                    ret = __st7789_write_data_dma(driver_instance,
                                                  s_fill_tile_buf,
                                                  ST7789_FILL_TILE_BYTES);
                    if (ST7789_OK != ret)
                    {
                        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                                  "draw_char dma flush failed = %d",
                                  (int)ret);
                        break;
                    }
                    tile_pixels = 0U;
                }
            }

            if (ST7789_OK != ret)
            {
                break;
            }
        }

        if ((ST7789_OK == ret) && (tile_pixels > 0U))
        {
            ret = __st7789_write_data_dma(driver_instance,
                                          s_fill_tile_buf,
                                          tile_pixels * 2U);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                          "draw_char dma tail failed = %d", (int)ret);
            }
        }

        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(ST7789_PIN_HIGH);
    }

    return ret;
}

/**
 * @brief  Draw a null-terminated ASCII string.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x, y            : Start coordinate of the first glyph.
 * @param[in] str             : NUL-terminated ASCII string.
 * @param[in] color, bg_color : Foreground / background RGB565.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_string(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                      x,
                                               uint16_t                      y,
                                                   char  const*            str,
                                               uint16_t                  color,
                                               uint16_t               bg_color)
{
    st7789_status_t ret = ST7789_OK;
    const font_def_t *font = &Font_7x10_t;
    uint32_t cursor_x = (uint32_t)x;

    if ((NULL == driver_instance) || (NULL == str))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_string invalid argument");
        return ST7789_ERRORPARAMETER;
    }

    if ((y >= driver_instance->panel.height)                               ||
        ((uint32_t)y + (uint32_t)font->height >
         (uint32_t)driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_string y out of range y=%u", y);
        return ST7789_ERRORPARAMETER;
    }

    for (uint32_t idx = 0U; '\0' != str[idx]; idx++)
    {
        if (cursor_x + (uint32_t)font->width >
            (uint32_t)driver_instance->panel.width)
        {
            DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                      "draw_string x overflow at char=%u", (unsigned)idx);
            return ST7789_ERRORPARAMETER;
        }

        ret = st7789_draw_char(driver_instance,
                               (uint16_t)cursor_x,
                               y,
                               str[idx],
                               color,
                               bg_color);
        if (ST7789_OK != ret)
        {
            DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                      "draw_string draw_char failed = %d", (int)ret);
            return ret;
        }

        cursor_x += (uint32_t)font->width;
    }

    return ret;
}

/**
 * @brief  Draw a solid-filled rectangle.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x0, y0, x1, y1  : Inclusive corner coordinates.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_filled_rectangle(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                     x0,
                                               uint16_t                     y0,
                                               uint16_t                     x1,
                                               uint16_t                     y1,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_filled_rectangle null driver");
        return ST7789_ERRORPARAMETER;
    }

    {
        const uint16_t x_start = (x0 < x1) ? x0 : x1;
        const uint16_t y_start = (y0 < y1) ? y0 : y1;
        const uint16_t x_end = (x0 < x1) ? x1 : x0;
        const uint16_t y_end = (y0 < y1) ? y1 : y0;

        ret = st7789_fill_region(driver_instance,
                                 x_start, y_start,
                                 x_end, y_end,
                                 color);
    }

    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_filled_rectangle failed = %d", (int)ret);
    }

    return ret;
}

/**
 * @brief  Draw a solid-filled triangle.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] x0..y2          : Three vertices.
 * @param[in] color           : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_filled_triangle(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t                     x0,
                                               uint16_t                     y0,
                                               uint16_t                     x1,
                                               uint16_t                     y1,
                                               uint16_t                     x2,
                                               uint16_t                     y2,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_filled_triangle null driver");
        return ST7789_ERRORPARAMETER;
    }

    if ((x0 >= driver_instance->panel.width) ||
        (x1 >= driver_instance->panel.width) ||
        (x2 >= driver_instance->panel.width) ||
        (y0 >= driver_instance->panel.height) ||
        (y1 >= driver_instance->panel.height) ||
        (y2 >= driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_filled_triangle vertex out of range");
        return ST7789_ERRORPARAMETER;
    }

    {
        int32_t x0_i = (int32_t)x0;
        int32_t y0_i = (int32_t)y0;
        int32_t x1_i = (int32_t)x1;
        int32_t y1_i = (int32_t)y1;
        int32_t x2_i = (int32_t)x2;
        int32_t y2_i = (int32_t)y2;

        if (y0_i > y1_i)
        {
            int32_t tmp = x0_i;
            x0_i = x1_i;
            x1_i = tmp;

            tmp = y0_i;
            y0_i = y1_i;
            y1_i = tmp;
        }
        if (y1_i > y2_i)
        {
            int32_t tmp = x1_i;
            x1_i = x2_i;
            x2_i = tmp;

            tmp = y1_i;
            y1_i = y2_i;
            y2_i = tmp;
        }
        if (y0_i > y1_i)
        {
            int32_t tmp = x0_i;
            x0_i = x1_i;
            x1_i = tmp;

            tmp = y0_i;
            y0_i = y1_i;
            y1_i = tmp;
        }

        if (y0_i == y2_i)
        {
            int32_t x_min = x0_i;
            int32_t x_max = x0_i;

            if (x1_i < x_min)
            {
                x_min = x1_i;
            }
            if (x2_i < x_min)
            {
                x_min = x2_i;
            }
            if (x1_i > x_max)
            {
                x_max = x1_i;
            }
            if (x2_i > x_max)
            {
                x_max = x2_i;
            }

            ret = st7789_fill_region(driver_instance,
                                     (uint16_t)x_min,
                                     (uint16_t)y0_i,
                                     (uint16_t)x_max,
                                     (uint16_t)y0_i,
                                     color);
            return ret;
        }

        for (int32_t y = y0_i; y <= y2_i; y++)
        {
            const int32_t xa = x0_i + ((x2_i - x0_i) * (y - y0_i)) / (y2_i - y0_i);
            int32_t xb = x1_i;

            if (y < y1_i)
            {
                if (y1_i != y0_i)
                {
                    xb = x0_i + ((x1_i - x0_i) * (y - y0_i)) / (y1_i - y0_i);
                }
            }
            else
            {
                if (y2_i != y1_i)
                {
                    xb = x1_i + ((x2_i - x1_i) * (y - y1_i)) / (y2_i - y1_i);
                }
            }

            {
                const uint16_t x_start = (uint16_t)((xa < xb) ? xa : xb);
                const uint16_t x_end   = (uint16_t)((xa < xb) ? xb : xa);

                ret = st7789_fill_region(driver_instance,
                                         x_start,
                                         (uint16_t)y,
                                         x_end,
                                         (uint16_t)y,
                                         color);
                if (ST7789_OK != ret)
                {
                    DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                              "draw_filled_triangle span failed = %d",
                              (int)ret);
                    return ret;
                }
            }
        }
    }

    return ret;
}

/**
 * @brief  Draw a solid-filled circle.
 *
 * @param[in] driver_instance    : Driver object.
 * @param[in] x_center, y_center : Circle center.
 * @param[in] radius             : Radius in pixels.
 * @param[in] color              : RGB565 pixel value.
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_draw_filled_circle(
                                    bsp_st7789_driver_t *const driver_instance,
                                               uint16_t               x_center,
                                               uint16_t               y_center,
                                               uint16_t                 radius,
                                               uint16_t                  color)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_filled_circle null driver");
        return ST7789_ERRORPARAMETER;
    }

    if ((x_center >= driver_instance->panel.width) ||
        (y_center >= driver_instance->panel.height))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "draw_filled_circle center out of range (%u, %u)",
                  x_center, y_center);
        return ST7789_ERRORPARAMETER;
    }

    {
        const int32_t x_min = (int32_t)x_center - (int32_t)radius;
        const int32_t y_min = (int32_t)y_center - (int32_t)radius;
        const int32_t x_max = (int32_t)x_center + (int32_t)radius;
        const int32_t y_max = (int32_t)y_center + (int32_t)radius;

        if ((x_min < 0) ||
            (y_min < 0) ||
            (x_max >= (int32_t)driver_instance->panel.width) ||
            (y_max >= (int32_t)driver_instance->panel.height))
        {
            DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                      "draw_filled_circle radius out of range r=%u", radius);
            return ST7789_ERRORPARAMETER;
        }
    }

    if (0U == radius)
    {
        return st7789_draw_pixel(driver_instance, x_center, y_center, color);
    }

    {
        int32_t x = (int32_t)radius;
        int32_t y = 0;
        int32_t err = 1 - x;

        while (x >= y)
        {
            ret = st7789_fill_region(driver_instance,
                                     (uint16_t)((int32_t)x_center - x),
                                     (uint16_t)((int32_t)y_center + y),
                                     (uint16_t)((int32_t)x_center + x),
                                     (uint16_t)((int32_t)y_center + y),
                                     color);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                          "draw_filled_circle span failed = %d", (int)ret);
                return ret;
            }

            if (y != 0)
            {
                ret = st7789_fill_region(driver_instance,
                                         (uint16_t)((int32_t)x_center - x),
                                         (uint16_t)((int32_t)y_center - y),
                                         (uint16_t)((int32_t)x_center + x),
                                         (uint16_t)((int32_t)y_center - y),
                                         color);
                if (ST7789_OK != ret)
                {
                    DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                              "draw_filled_circle span failed = %d", (int)ret);
                    return ret;
                }
            }

            if (x != y)
            {
                ret = st7789_fill_region(driver_instance,
                                         (uint16_t)((int32_t)x_center - y),
                                         (uint16_t)((int32_t)y_center + x),
                                         (uint16_t)((int32_t)x_center + y),
                                         (uint16_t)((int32_t)y_center + x),
                                         color);
                if (ST7789_OK != ret)
                {
                    DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                              "draw_filled_circle span failed = %d", (int)ret);
                    return ret;
                }

                ret = st7789_fill_region(driver_instance,
                                         (uint16_t)((int32_t)x_center - y),
                                         (uint16_t)((int32_t)y_center - x),
                                         (uint16_t)((int32_t)x_center + y),
                                         (uint16_t)((int32_t)y_center - x),
                                         color);
                if (ST7789_OK != ret)
                {
                    DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                              "draw_filled_circle span failed = %d", (int)ret);
                    return ret;
                }
            }

            y++;
            if (err < 0)
            {
                err += (2 * y) + 1;
            }
            else
            {
                x--;
                err += (2 * (y - x)) + 1;
            }
        }
    }

    return ret;
}

/**
 * @brief  Enable / disable tearing-effect (TE) output on the TE pin.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] enable          : Non-zero enables TE (TEON), zero disables (TEOFF).
 *
 * @return ST7789_OK on success, error code otherwise.
 * */
static st7789_status_t st7789_tear_effect(
                                    bsp_st7789_driver_t *const driver_instance,
                                                uint8_t                 enable)
{
    st7789_status_t ret = ST7789_OK;
    if (NULL == driver_instance)
    {
        ret = ST7789_ERRORPARAMETER;
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "tear_effect null driver");
        return ret;
    }

    ret = __st7789_write_command(driver_instance,
                                  (0U != enable) ? ST7789_TEON : ST7789_TEOFF);

    return ret;
}

/**
 * @brief  Instantiate a ST7789 driver object: validate caller-supplied
 *         interfaces and panel geometry, bind them into the driver instance,
 *         and mount the public API vtable.  Does NOT touch the hardware -
 *         the caller must invoke pf_st7789_init() afterwards to run the
 *         power-on reset and register-configuration sequence.
 *
 * @param[out] driver_instance         Driver object to populate.
 * @param[in]  p_spi_interface         Raw SPI / CS / DC / RST / DMA vtable.
 * @param[in]  p_spi_driver_interface  ST7789 framing vtable
 *                                     (write_cmd / write_data wrappers).
 * @param[in]  p_timebase_interface    ms-tick / busy-wait delay vtable.
 * @param[in]  p_os_interface          OS-aware delay vtable.
 * @param[in]  p_panel                 Panel geometry (width/height/offsets).
 *
 * @return  ST7789_OK                  - Success.
 *          ST7789_ERRORPARAMETER      - NULL pointer or out-of-range geometry.
 *          ST7789_ERRORRESOURCE       - Instance already initialized, or a
 *                                       required vtable slot is NULL.
 * */
st7789_status_t bsp_st7789_driver_inst(
                                   bsp_st7789_driver_t * const driver_instance,
                                st7789_spi_interface_t *       p_spi_interface,
                           st7789_timebase_interface_t *  p_timebase_interface,
                                 st7789_os_interface_t *        p_os_interface,
                           const st7789_panel_config_t *               p_panel
                                        )
{
    DEBUG_OUT(i, ST7789_LOG_TAG, "bsp_st7789_driver_inst start");

    st7789_status_t ret = ST7789_OK;

    /************ 1.Checking input parameters ************/
    /**
     * All top-level pointers are mandatory.  p_os_interface is required as
     * well because the project always links against FreeRTOS; a bare-metal
     * variant would gate it with OS_SUPPORTING, which this driver does not
     * expose.
     **/
    if (NULL == driver_instance                          ||
        NULL == p_spi_interface                          ||
        NULL == p_timebase_interface                     ||
        NULL == p_os_interface                           ||
        NULL == p_panel)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "bsp_st7789_driver_inst input error parameter");
        ret = ST7789_ERRORPARAMETER;
        return ret;
    }

    /**
     * Panel geometry sanity check.  width/height == 0 would make every draw
     * call a no-op and any fill_region compute wrong addr windows, so treat
     * it as a caller error rather than silently accepting it.  Upper bound
     * follows ST7789 GRAM limits.
     **/
    if ((0U == p_panel->width)                           ||
        (0U == p_panel->height)                          ||
        (p_panel->width  > ST7789_MAX_PANEL_WIDTH)       ||
        (p_panel->height > ST7789_MAX_PANEL_HEIGHT))
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "bsp_st7789_driver_inst panel geometry out of range");
        ret = ST7789_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    /**
     * Reject re-instantiation of the same driver object: re-entering this
     * helper after a successful bind would overwrite bookkeeping and could
     * double-init the underlying SPI bus.
     **/
    if (ST7789_IS_INITED == driver_instance->is_init)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "bsp_st7789_driver_inst instance already initialized");
        ret = ST7789_ERRORRESOURCE;
        return ret;
    }

    /**
     * Verify every vtable slot the driver will actually call.  Do this on
     * the caller-supplied pointers directly, BEFORE mounting them onto the
     * driver instance, so a partial bind never leaks into the instance on
     * failure.
     **/
    if (NULL == p_spi_interface->pf_spi_init             ||
        NULL == p_spi_interface->pf_spi_deinit           ||
        NULL == p_spi_interface->pf_spi_transmit         ||
        NULL == p_spi_interface->pf_spi_transmit_dma     ||
        NULL == p_spi_interface->pf_spi_wait_dma_complete||
        NULL == p_spi_interface->pf_spi_write_cs_pin     ||
        NULL == p_spi_interface->pf_spi_write_dc_pin     ||
        NULL == p_spi_interface->pf_spi_write_rst_pin)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "st7789 spi_interface has NULL callback");
        ret = ST7789_ERRORRESOURCE;
        return ret;
    }


    if (NULL == p_timebase_interface->pf_get_tick_ms     ||
        NULL == p_timebase_interface->pf_delay_ms)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "st7789 timebase_interface has NULL callback");
        ret = ST7789_ERRORRESOURCE;
        return ret;
    }

    if (NULL == p_os_interface->pf_os_delay_ms)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "st7789 os_interface has NULL callback");
        ret = ST7789_ERRORRESOURCE;
        return ret;
    }

    /************** 3.Mount the Interfaces ***************/
    /**
     * 3.1 Mount external interfaces.
     * Bind all caller-supplied vtables and geometry onto the driver
     * instance so subsequent API calls can dereference them via
     * driver_instance->p_xxx and driver_instance->panel.
     **/
    driver_instance->p_spi_interface                 =         p_spi_interface;
    driver_instance->p_timebase_interface            =    p_timebase_interface;
    driver_instance->p_os_interface                  =          p_os_interface;
    driver_instance->panel                           =                *p_panel;

    /**
     * 3.2 Mount internal API vtable.
     * Each slot points to a file-local stub; call-sites dispatch via
     * driver_instance->pf_xxx(driver_instance, ...).  Stubs currently
     * only acknowledge the call and return ST7789_OK; real command
     * sequences will fill them in incrementally.
     **/
    driver_instance->pf_st7789_init                  =             st7789_init;
    driver_instance->pf_st7789_deinit                =           st7789_deinit;
    driver_instance->pf_st7789_fill_color            =       st7789_fill_color;
    driver_instance->pf_st7789_draw_pixel            =       st7789_draw_pixel;
    driver_instance->pf_st7789_fill_region           =      st7789_fill_region;
    driver_instance->pf_st7789_draw_pixel_4px        =   st7789_draw_pixel_4px;
    driver_instance->pf_st7789_draw_line             =        st7789_draw_line;
    driver_instance->pf_st7789_draw_rectangle        =   st7789_draw_rectangle;
    driver_instance->pf_st7789_draw_circle           =      st7789_draw_circle;
    driver_instance->pf_st7789_draw_image            =       st7789_draw_image;
    driver_instance->pf_invert_colors                =           invert_colors;
    driver_instance->pf_st7789_draw_char             =        st7789_draw_char;
    driver_instance->pf_st7789_draw_string           =      st7789_draw_string;
    driver_instance->pf_st7789_draw_filled_rectangle =
                                                  st7789_draw_filled_rectangle;
    driver_instance->pf_st7789_draw_filled_triangle  =
                                                   st7789_draw_filled_triangle;
    driver_instance->pf_st7789_draw_filled_circle    =
                                                     st7789_draw_filled_circle;
    driver_instance->pf_st7789_tear_effect           =      st7789_tear_effect;

    DEBUG_OUT(d, ST7789_LOG_TAG, "bsp_st7789_driver_inst success");
    return ret;
}

//******************************* Functions *********************************//
