/******************************************************************************
 * @file cfg_touch.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Project-level configuration for the capacitive touch panel and the
 *        9-point on-screen calibration flow.
 *
 *        Single source of truth for:
 *          - panel resolution (used by lv_port_indev clamp, calibration
 *            apply boundary, calibration UI standard-point geometry, and
 *            the panel_w/panel_h stored alongside saved coefficients)
 *          - calibration grid geometry (point count + margin) so the 9 cross
 *            positions are derived at runtime, not hard-coded
 *          - persistence magic / schema version / W25Q64 in-region offset
 *
 *        Changing CFG_TOUCH_PANEL_WIDTH / CFG_TOUCH_PANEL_HEIGHT propagates
 *        automatically through:
 *          - lv_port_indev.c clamp limits
 *          - bsp_cst816t_calibration apply boundary clamp
 *          - touch_calibration_get_standard_point (9-cross positions)
 *          - touch_calibration_load_from_storage panel_w/panel_h guard which
 *            invalidates a stale calibration so the user is re-prompted
 *
 *        Hardware ceiling: CST816T can address up to 240x320 and ST7789
 *        drives a 240x320 pixel matrix, so the static asserts below cap the
 *        configurable values to that range.
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __CFG_TOUCH_H__
#define __CFG_TOUCH_H__

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* === Panel resolution (single source of truth) =========================== *
 * Override at build time with -DCFG_TOUCH_PANEL_WIDTH=<n> if the LCD module
 * is re-flashed to a different geometry.  The ST7789 driver init must be
 * updated to match — this header does not drive the controller registers. */
#ifndef CFG_TOUCH_PANEL_WIDTH
#define CFG_TOUCH_PANEL_WIDTH        (240U)
#endif
#ifndef CFG_TOUCH_PANEL_HEIGHT
#define CFG_TOUCH_PANEL_HEIGHT       (284U)
#endif

_Static_assert(CFG_TOUCH_PANEL_WIDTH  <= 240U,
               "CFG_TOUCH_PANEL_WIDTH exceeds CST816T addressable range");
_Static_assert(CFG_TOUCH_PANEL_HEIGHT <= 320U,
               "CFG_TOUCH_PANEL_HEIGHT exceeds ST7789 / CST816T range");
_Static_assert(CFG_TOUCH_PANEL_WIDTH  >= 80U,
               "CFG_TOUCH_PANEL_WIDTH too small for a sane calibration grid");
_Static_assert(CFG_TOUCH_PANEL_HEIGHT >= 80U,
               "CFG_TOUCH_PANEL_HEIGHT too small for a sane calibration grid");

/* === 9-point calibration geometry (all derived from W/H + margin) ======== */
#define CFG_TOUCH_CALIB_POINT_COUNT  (9U)        /* 3 x 3 grid              */
#define CFG_TOUCH_CALIB_MARGIN       (30U)       /* px from edge to outer    */
                                                 /* row / column             */
#define CFG_TOUCH_CALIB_TIMEOUT_MS   (30000U)    /* per-point timeout        */

/* Sanity: margin must leave room for the cross + a safety pad. */
_Static_assert(CFG_TOUCH_CALIB_MARGIN * 2U + 40U <= CFG_TOUCH_PANEL_WIDTH,
               "CFG_TOUCH_CALIB_MARGIN too large for panel width");
_Static_assert(CFG_TOUCH_CALIB_MARGIN * 2U + 40U <= CFG_TOUCH_PANEL_HEIGHT,
               "CFG_TOUCH_CALIB_MARGIN too large for panel height");

/* === Persistence (W25Q64 calibration block) ============================== *
 * Region base is service_storage's MEMORY_CALIB_START_ADDRESS; this header
 * only defines the in-region layout and the validation tokens. */
#define CFG_TOUCH_CALIB_MAGIC        (0xCABCABCAUL)
#define CFG_TOUCH_CALIB_SCHEMA_VER   (0x0001U)
#define CFG_TOUCH_CALIB_OFFSET       (0x000000UL) /* offset in CALIB region   */

/* === Debug / bring-up: force calibration on every boot =================== *
 * Set to 1 to make touch_calibration_boot_apply() skip the storage load and
 * launch the calibration UI unconditionally — useful during testing when
 * there is no runtime way to invalidate the saved coefficients.  Leave at 0
 * for normal operation: the UI only fires on magic / CRC / panel mismatch. */
#ifndef CFG_TOUCH_CALIB_FORCE_ON_BOOT
#define CFG_TOUCH_CALIB_FORCE_ON_BOOT  (1)
#endif
//******************************** Defines **********************************//

#endif /* __CFG_TOUCH_H__ */
