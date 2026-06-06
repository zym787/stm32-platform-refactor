/******************************************************************************
 * @file bsp_cst816t_calibration.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - cfg_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief 9-point on-screen calibration for the capacitive touch panel.
 *
 *        Algorithm: full 6-parameter affine least-squares fit
 *
 *            screen_x = a * raw_x + b * raw_y + c
 *            screen_y = d * raw_x + e * raw_y + f
 *
 *        Coefficients are solved per row by inverting the 3x3 normal-equation
 *        matrix with Cramer's rule (cheap on a Cortex-M4F single-precision
 *        FPU).  A near-singular determinant falls back to the simplified
 *        independent X/Y linear regression used by the reference firmware
 *        (ec_s100_watch_1) so a partially-collected calibration still does
 *        something useful.
 *
 *        Persistence: a single 4-KB W25Q64 sector dedicated to the affine
 *        coefficients plus a panel_w/panel_h tag (see cfg_touch.h for the
 *        in-region layout).  Save / load go through the service_storage
 *        facade (Read_/Write_CalibData).  A magic + CRC32 + panel-resolution
 *        check at load time invalidates stale data when the panel geometry
 *        is changed at build time, which forces an automatic re-calibration.
 *
 * Processing flow:
 *   1. Boot-time orchestrator calls touch_calibration_load_from_storage().
 *   2. On magic / CRC / resolution mismatch, the calibration UI samples
 *      9 (raw -> screen) pairs and feeds them through
 *      touch_calibration_add_point().
 *   3. touch_calibration_calculate_affine() solves for the 6 coefficients.
 *   4. touch_calibration_save_to_storage() persists the result.
 *   5. Steady-state lv_port_indev applies touch_calibration_apply_matrix()
 *      on every raw sample.
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_CST816T_CALIBRATION_H__
#define __BSP_CST816T_CALIBRATION_H__

//******************************** Includes *********************************//
#include "board_types.h"

#include "cfg_touch.h"
//******************************** Includes *********************************//

//******************************** Typedefs *********************************//
/**
 * @brief One sampled calibration pair.  raw_* are the controller's reported
 *        coordinates, screen_* are the expected on-screen position the UI
 *        prompted the user to touch.
 */
typedef struct
{
    UINT16_t raw_x;
    UINT16_t raw_y;
    UINT16_t screen_x;
    UINT16_t screen_y;
} calibration_point_t;

/**
 * @brief Calibration state: collected samples + solved coefficients +
 *        an is_calibrated flag the indev port consults on every read.
 *
 *        Layout is internal — callers should treat the struct as opaque and
 *        go through the API functions below.
 */
typedef struct
{
    BOOL                 is_calibrated;
    calibration_point_t  points[CFG_TOUCH_CALIB_POINT_COUNT];

    /* Affine coefficients:  X' = a*x + b*y + c,  Y' = d*x + e*y + f */
    FLOAT matrix_a;
    FLOAT matrix_b;
    FLOAT matrix_c;
    FLOAT matrix_d;
    FLOAT matrix_e;
    FLOAT matrix_f;

    /* Panel resolution the calibration was captured at — used by load to
     * detect a build-time resolution change and force a re-calibration. */
    UINT16_t panel_w;
    UINT16_t panel_h;
} touch_calibration_t;

/**
 * @brief API status codes.  Mapped to log severity by the caller.
 */
typedef enum
{
    CALIBRATION_SUCCESS                  = 0,
    CALIBRATION_ERROR_INVALID_POINT,
    CALIBRATION_ERROR_INSUFFICIENT_POINTS,
    CALIBRATION_ERROR_CALCULATION_FAILED,
    CALIBRATION_ERROR_STORAGE_FAILED,
    CALIBRATION_ERROR_MAGIC_MISMATCH,
    CALIBRATION_ERROR_CRC_MISMATCH,
    CALIBRATION_ERROR_PANEL_MISMATCH,
} calibration_status_t;
//******************************** Typedefs *********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialise the calibration context to the "uncalibrated" state.
 *        Coefficients are seeded to identity so that apply_matrix() before
 *        calibration acts as a no-op pass-through.
 */
calibration_status_t touch_calibration_init(touch_calibration_t *p_calibration);

/**
 * @brief Record one sampled (raw, screen) pair at a specific point index.
 *
 * @param[in] idx  Slot in 0 .. CFG_TOUCH_CALIB_POINT_COUNT-1.
 */
calibration_status_t touch_calibration_add_point(
        touch_calibration_t *p_calibration,
        UINT16_t raw_x, UINT16_t raw_y,
        UINT16_t screen_x, UINT16_t screen_y,
        UINT8_t  idx);

/**
 * @brief Solve the 6 affine coefficients via least-squares.
 *
 *        Builds normal equations for the X and Y rows independently and
 *        inverts each 3x3 system with Cramer's rule.  When the determinant
 *        is near zero (degenerate sample set), falls back to the simplified
 *        independent X/Y linear regression (matrix_b/d clamped to 0).
 */
calibration_status_t touch_calibration_calculate_affine(
        touch_calibration_t *p_calibration);

/**
 * @brief Apply the current coefficients to a raw (x, y) sample.
 *
 *        Output is clamped to the panel rectangle (CFG_TOUCH_PANEL_WIDTH x
 *        CFG_TOUCH_PANEL_HEIGHT).  If is_calibrated is false, raw values
 *        are passed through unchanged.
 */
calibration_status_t touch_calibration_apply_matrix(
        touch_calibration_t *p_calibration,
        UINT16_t raw_x, UINT16_t raw_y,
        UINT16_t *p_screen_x, UINT16_t *p_screen_y);

/**
 * @brief Persist the current coefficients to W25Q64 calibration sector.
 *
 *        Writes the full 4-KB sector (header + coefficients + padding).
 *        Returns CALIBRATION_ERROR_STORAGE_FAILED if the underlying
 *        Write_CalibData call fails.
 */
calibration_status_t touch_calibration_save_to_storage(
        touch_calibration_t *p_calibration);

/**
 * @brief Reload coefficients from W25Q64 calibration sector.
 *
 *        Validates magic + schema_version + CRC32 + panel_w/panel_h.  On any
 *        mismatch returns the matching error code (callers treat any non-
 *        SUCCESS as "uncalibrated" and trigger the UI).
 */
calibration_status_t touch_calibration_load_from_storage(
        touch_calibration_t *p_calibration);

/**
 * @brief Compute the screen-coordinate of the i-th standard calibration
 *        cross.  Geometry is derived at runtime from CFG_TOUCH_PANEL_WIDTH /
 *        CFG_TOUCH_PANEL_HEIGHT / CFG_TOUCH_CALIB_MARGIN; no hard-coded
 *        coordinate table.
 *
 *        Index mapping (3 x 3 grid, row-major from top-left):
 *
 *            0  1  2     row 0 (top,    y = margin)
 *            3  4  5     row 1 (middle, y = H / 2)
 *            6  7  8     row 2 (bottom, y = H - 1 - margin)
 */
calibration_status_t touch_calibration_get_standard_point(
        UINT8_t   idx,
        UINT16_t *p_screen_x,
        UINT16_t *p_screen_y);

/**
 * @brief Reset back to the uncalibrated identity state.
 */
calibration_status_t touch_calibration_reset(
        touch_calibration_t *p_calibration);

/**
 * @brief Process-wide singleton accessor.  The indev port and the UI share
 *        this same instance.
 */
touch_calibration_t *touch_calibration_get_instance(void);

/**
 * @brief Convenience check used by lv_port_indev to decide whether to apply
 *        the affine transform on a given read.
 */
BOOL touch_calibration_is_calibrated(void);
//******************************* Functions *********************************//

#endif /* __BSP_CST816T_CALIBRATION_H__ */
