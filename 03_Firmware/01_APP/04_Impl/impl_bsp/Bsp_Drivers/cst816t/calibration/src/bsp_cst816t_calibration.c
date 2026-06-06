/******************************************************************************
 * @file bsp_cst816t_calibration.c
 *
 * @par dependencies
 * - bsp_cst816t_calibration.h
 * - cfg_touch.h
 * - service_storage_facade.h
 *
 * @author Ethan-Hang
 *
 * @brief 9-point affine calibration algorithm + persistence (W25Q64 via
 *        service_storage facade).
 *
 *        Algorithm reference: ec_s100_watch_1(2)'s bsp_cst816t_calibration.c,
 *        adapted from independent X/Y regression to full 6-parameter affine
 *        least squares so a slightly-rotated panel install can be corrected.
 *
 *        Normal equations (per axis, 3 unknowns):
 *
 *            [ Sxx  Sxy  Sx ] [a]   [ SxX ]
 *            [ Sxy  Syy  Sy ] [b] = [ SyX ]
 *            [ Sx   Sy   n  ] [c]   [ SX  ]
 *
 *        Solved via Cramer's rule on a 3x3 determinant.  When det approaches
 *        zero (sample set falls on a line), drops back to ec_s100's
 *        independent X/Y regression so a half-broken calibration still
 *        produces something usable.
 *
 *        Persistence layout in MEMORY_CALIB (single W25Q64 4 KB sector):
 *
 *            offset 0x000  4 B   magic    = CFG_TOUCH_CALIB_MAGIC
 *            offset 0x004  2 B   version  = CFG_TOUCH_CALIB_SCHEMA_VER
 *            offset 0x006  2 B   reserved (kept 0xFFFF)
 *            offset 0x008  4 B   crc32(payload)  IEEE-802.3 reflected
 *            offset 0x00C  28 B  touch_calibration_persist_t
 *            offset 0x028~ 0xFF padding
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "board_types.h"
#include "bsp_cst816t_calibration.h"

#include "service_storage_facade.h"
#include "Debug.h"

#include <math.h>
#include <string.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define CALIB_SECTOR_SIZE       (4096U)
#define CALIB_HEADER_OFFSET     (0x000UL)
#define CALIB_PAYLOAD_OFFSET    (0x00CUL)

/* Determinant epsilon for the 3x3 solve.  Below this the matrix is treated
 * as singular and we fall back to independent X/Y regression. */
#define CALIB_DET_EPS           (1.0e-6f)
//******************************** Defines **********************************//

//******************************** Typedefs *********************************//
/**
 * @brief On-disk header for the calibration sector.  Fixed at 12 bytes so the
 *        payload starts on a 4-byte boundary.
 */
typedef struct
{
    UINT32_t magic;
    UINT16_t schema_version;
    UINT16_t reserved;
    UINT32_t crc32;
} __attribute__((packed)) calibration_header_t;

/**
 * @brief Persisted body — 6 affine coefficients + the panel resolution they
 *        were captured at.  32 bytes (the panel_w/h pair is 4 B aligned).
 */
typedef struct
{
    FLOAT    matrix_a;
    FLOAT    matrix_b;
    FLOAT    matrix_c;
    FLOAT    matrix_d;
    FLOAT    matrix_e;
    FLOAT    matrix_f;
    UINT16_t panel_w;
    UINT16_t panel_h;
} __attribute__((packed)) touch_calibration_persist_t;
//******************************** Typedefs *********************************//

//******************************* Variables *********************************//
static touch_calibration_t s_calibration;
//******************************* Variables *********************************//

//******************************* Declaring *********************************//
static UINT32_t calib_crc32(const UINT8_t *data, UINT32_t len);
static void     identity_seed(touch_calibration_t *p);
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Bit-wise CRC32 (reflected IEEE 802.3, poly 0xEDB88320).
 *
 *        Tableless — payload is only 32 bytes so 256 iterations is trivial.
 *        Avoids dragging in a 1 KB lookup table just for this one consumer.
 */
static UINT32_t calib_crc32(const UINT8_t *data, UINT32_t len)
{
    UINT32_t crc = 0xFFFFFFFFUL;
    for (UINT32_t i = 0U; i < len; ++i)
    {
        crc ^= (UINT32_t)data[i];
        for (UINT8_t b = 0U; b < 8U; ++b)
        {
            UINT32_t mask = (crc & 1U) ? 0xEDB88320UL : 0U;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

/**
 * @brief Seed the calibration struct with identity coefficients so that
 *        apply_matrix on an uncalibrated instance returns raw == screen.
 */
static void identity_seed(touch_calibration_t *p)
{
    p->matrix_a = 1.0f;
    p->matrix_b = 0.0f;
    p->matrix_c = 0.0f;
    p->matrix_d = 0.0f;
    p->matrix_e = 1.0f;
    p->matrix_f = 0.0f;
}

calibration_status_t touch_calibration_init(touch_calibration_t *p_calibration)
{
    if (NULL == p_calibration)
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    memset(p_calibration, 0, sizeof(*p_calibration));
    identity_seed(p_calibration);
    p_calibration->is_calibrated = false;
    p_calibration->panel_w       = CFG_TOUCH_PANEL_WIDTH;
    p_calibration->panel_h       = CFG_TOUCH_PANEL_HEIGHT;

    return CALIBRATION_SUCCESS;
}

calibration_status_t touch_calibration_reset(touch_calibration_t *p_calibration)
{
    return touch_calibration_init(p_calibration);
}

calibration_status_t touch_calibration_add_point(
        touch_calibration_t *p_calibration,
        UINT16_t raw_x, UINT16_t raw_y,
        UINT16_t screen_x, UINT16_t screen_y,
        UINT8_t  idx)
{
    if ((NULL == p_calibration) || (idx >= CFG_TOUCH_CALIB_POINT_COUNT))
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    p_calibration->points[idx].raw_x    = raw_x;
    p_calibration->points[idx].raw_y    = raw_y;
    p_calibration->points[idx].screen_x = screen_x;
    p_calibration->points[idx].screen_y = screen_y;

    return CALIBRATION_SUCCESS;
}

/**
 * @brief Solve [ Sxx Sxy Sx ; Sxy Syy Sy ; Sx Sy n ] * (a, b, c) = (Sxv, Syv, Sv)
 *        with Cramer's rule.  Writes the three components into (*a, *b, *c).
 *
 *        Returns the determinant (so the caller can decide if the result is
 *        meaningful).  All inputs are FLOAT to keep precision under control
 *        of the F411 single-precision FPU.
 */
static FLOAT solve_3x3_normal(FLOAT Sxx, FLOAT Sxy, FLOAT Sx,
                              FLOAT Syy, FLOAT Sy,  FLOAT n,
                              FLOAT Sxv, FLOAT Syv, FLOAT Sv,
                              FLOAT *a,  FLOAT *b,  FLOAT *c)
{
    /* | Sxx  Sxy  Sx |
     * | Sxy  Syy  Sy |
     * | Sx   Sy   n  |
     */
    const FLOAT det = Sxx * (Syy * n  - Sy  * Sy)
                    - Sxy * (Sxy * n  - Sy  * Sx)
                    + Sx  * (Sxy * Sy - Syy * Sx);

    if (fabsf(det) < CALIB_DET_EPS)
    {
        return det;
    }

    const FLOAT det_a = Sxv * (Syy * n  - Sy  * Sy)
                      - Sxy * (Syv * n  - Sy  * Sv)
                      + Sx  * (Syv * Sy - Syy * Sv);

    const FLOAT det_b = Sxx * (Syv * n  - Sy  * Sv)
                      - Sxv * (Sxy * n  - Sy  * Sx)
                      + Sx  * (Sxy * Sv - Syv * Sx);

    const FLOAT det_c = Sxx * (Syy * Sv - Sy  * Syv)
                      - Sxy * (Sxy * Sv - Sx  * Syv)
                      + Sxv * (Sxy * Sy - Syy * Sx);

    *a = det_a / det;
    *b = det_b / det;
    *c = det_c / det;

    return det;
}

calibration_status_t touch_calibration_calculate_affine(
        touch_calibration_t *p_calibration)
{
    if (NULL == p_calibration)
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    /* Aggregate samples — treat (0, 0) raw as "slot not populated" exactly
     * like the reference firmware does.  In practice the controller never
     * reports raw (0, 0) for a real touch on this panel. */
    FLOAT Sx  = 0.0f, Sy  = 0.0f;
    FLOAT Sxx = 0.0f, Syy = 0.0f, Sxy = 0.0f;
    FLOAT SXx = 0.0f, SYx = 0.0f;     /* sum of raw_x * screen_{x,y}  */
    FLOAT SXy = 0.0f, SYy = 0.0f;     /* sum of raw_y * screen_{x,y}  */
    FLOAT SX  = 0.0f, SY  = 0.0f;     /* sum of screen_{x, y}         */
    UINT8_t valid = 0U;

    for (UINT8_t i = 0U; i < CFG_TOUCH_CALIB_POINT_COUNT; ++i)
    {
        const calibration_point_t *pt = &p_calibration->points[i];
        if ((0U == pt->raw_x) && (0U == pt->raw_y))
        {
            continue;
        }

        const FLOAT tx = (FLOAT)pt->raw_x;
        const FLOAT ty = (FLOAT)pt->raw_y;
        const FLOAT sx = (FLOAT)pt->screen_x;
        const FLOAT sy = (FLOAT)pt->screen_y;

        Sx  += tx;     Sy  += ty;
        Sxx += tx*tx;  Syy += ty*ty;  Sxy += tx*ty;
        SX  += sx;     SY  += sy;
        SXx += tx*sx;  SYx += tx*sy;
        SXy += ty*sx;  SYy += ty*sy;

        ++valid;
    }

    if (valid < 4U)
    {
        DEBUG_OUT(e, TOUCH_CALIB_ERR_LOG_TAG,
                  "calib calc: too few valid pts (%u)", (unsigned)valid);
        return CALIBRATION_ERROR_INSUFFICIENT_POINTS;
    }

    const FLOAT n = (FLOAT)valid;

    /* Row X: screen_x = a * raw_x + b * raw_y + c */
    FLOAT a = 0.0f, b = 0.0f, c = 0.0f;
    const FLOAT det_x = solve_3x3_normal(Sxx, Sxy, Sx,
                                         Syy, Sy,  n,
                                         SXx, SXy, SX,
                                         &a,  &b,  &c);

    /* Row Y: screen_y = d * raw_x + e * raw_y + f */
    FLOAT d = 0.0f, e = 0.0f, f = 0.0f;
    const FLOAT det_y = solve_3x3_normal(Sxx, Sxy, Sx,
                                         Syy, Sy,  n,
                                         SYx, SYy, SY,
                                         &d,  &e,  &f);

    if ((fabsf(det_x) < CALIB_DET_EPS) || (fabsf(det_y) < CALIB_DET_EPS))
    {
        /* Fallback: independent X/Y regression (ec_s100 behaviour).  Cannot
         * correct rotation but still corrects scale + offset, which is the
         * dominant error on a typical install. */
        DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                  "calib calc: 3x3 singular (det_x=%.6f det_y=%.6f), "
                  "fallback to indep XY", (DOUBLE)det_x, (DOUBLE)det_y);

        const FLOAT denom_x = n * Sxx - Sx * Sx;
        const FLOAT denom_y = n * Syy - Sy * Sy;
        if ((fabsf(denom_x) < CALIB_DET_EPS) ||
            (fabsf(denom_y) < CALIB_DET_EPS))
        {
            return CALIBRATION_ERROR_CALCULATION_FAILED;
        }

        a = (n * SXx - Sx * SX) / denom_x;
        c = (SX - a * Sx) / n;
        b = 0.0f;

        e = (n * SYy - Sy * SY) / denom_y;
        f = (SY - e * Sy) / n;
        d = 0.0f;
    }

    p_calibration->matrix_a = a;
    p_calibration->matrix_b = b;
    p_calibration->matrix_c = c;
    p_calibration->matrix_d = d;
    p_calibration->matrix_e = e;
    p_calibration->matrix_f = f;
    p_calibration->panel_w  = CFG_TOUCH_PANEL_WIDTH;
    p_calibration->panel_h  = CFG_TOUCH_PANEL_HEIGHT;
    p_calibration->is_calibrated = true;

    DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG,
              "calib affine: a=%.4f b=%.4f c=%.2f d=%.4f e=%.4f f=%.2f",
              (DOUBLE)a, (DOUBLE)b, (DOUBLE)c,
              (DOUBLE)d, (DOUBLE)e, (DOUBLE)f);

    return CALIBRATION_SUCCESS;
}

calibration_status_t touch_calibration_apply_matrix(
        touch_calibration_t *p_calibration,
        UINT16_t raw_x, UINT16_t raw_y,
        UINT16_t *p_screen_x, UINT16_t *p_screen_y)
{
    if ((NULL == p_calibration) ||
        (NULL == p_screen_x)    || (NULL == p_screen_y))
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    if (!p_calibration->is_calibrated)
    {
        *p_screen_x = raw_x;
        *p_screen_y = raw_y;
        return CALIBRATION_SUCCESS;
    }

    const FLOAT tx = (FLOAT)raw_x;
    const FLOAT ty = (FLOAT)raw_y;

    FLOAT fx = p_calibration->matrix_a * tx
             + p_calibration->matrix_b * ty
             + p_calibration->matrix_c;
    FLOAT fy = p_calibration->matrix_d * tx
             + p_calibration->matrix_e * ty
             + p_calibration->matrix_f;

    /* Round, then clamp to panel rectangle.  Some samples around the edge
     * legitimately solve to one pixel past the boundary, which is fine —
     * the panel is the source of truth, anything outside is invalid. */
    if (fx < 0.0f)
    {
        fx = 0.0f;
    }
    else if (fx > (FLOAT)(CFG_TOUCH_PANEL_WIDTH - 1U))
    {
        fx = (FLOAT)(CFG_TOUCH_PANEL_WIDTH - 1U);
    }
    if (fy < 0.0f)
    {
        fy = 0.0f;
    }
    else if (fy > (FLOAT)(CFG_TOUCH_PANEL_HEIGHT - 1U))
    {
        fy = (FLOAT)(CFG_TOUCH_PANEL_HEIGHT - 1U);
    }

    *p_screen_x = (UINT16_t)(fx + 0.5f);
    *p_screen_y = (UINT16_t)(fy + 0.5f);
    return CALIBRATION_SUCCESS;
}

calibration_status_t touch_calibration_get_standard_point(
        UINT8_t idx,
        UINT16_t *p_screen_x,
        UINT16_t *p_screen_y)
{
    if ((NULL == p_screen_x) || (NULL == p_screen_y) ||
        (idx >= CFG_TOUCH_CALIB_POINT_COUNT))
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    const UINT16_t W = (UINT16_t)CFG_TOUCH_PANEL_WIDTH;
    const UINT16_t H = (UINT16_t)CFG_TOUCH_PANEL_HEIGHT;
    const UINT16_t M = (UINT16_t)CFG_TOUCH_CALIB_MARGIN;

    const UINT16_t col[3] = { M,
                              (UINT16_t)(W / 2U),
                              (UINT16_t)(W - 1U - M) };
    const UINT16_t row[3] = { M,
                              (UINT16_t)(H / 2U),
                              (UINT16_t)(H - 1U - M) };

    *p_screen_x = col[idx % 3U];
    *p_screen_y = row[idx / 3U];
    return CALIBRATION_SUCCESS;
}

/**
 * @brief Pack the in-RAM calibration into the small header+body image (no
 *        full-sector buffer needed — Write_CalibData triggers a sector erase
 *        underneath, so bytes past the payload stay 0xFF naturally).
 */
static void pack_image(const touch_calibration_t *p,
                       UINT8_t                   *image_buf,
                       UINT32_t                   image_len)
{
    /* image_buf is exactly CALIB_PAYLOAD_OFFSET + sizeof(persist_body). */
    (void)image_len;
    memset(image_buf, 0xFF, CALIB_PAYLOAD_OFFSET +
                            sizeof(touch_calibration_persist_t));

    calibration_header_t hdr;
    hdr.magic          = CFG_TOUCH_CALIB_MAGIC;
    hdr.schema_version = CFG_TOUCH_CALIB_SCHEMA_VER;
    hdr.reserved       = 0xFFFFU;
    hdr.crc32          = 0U;   /* placeholder, filled after payload pack */

    touch_calibration_persist_t body;
    body.matrix_a = p->matrix_a;
    body.matrix_b = p->matrix_b;
    body.matrix_c = p->matrix_c;
    body.matrix_d = p->matrix_d;
    body.matrix_e = p->matrix_e;
    body.matrix_f = p->matrix_f;
    body.panel_w  = p->panel_w;
    body.panel_h  = p->panel_h;

    memcpy(image_buf + CALIB_PAYLOAD_OFFSET, &body, sizeof(body));
    hdr.crc32 = calib_crc32(image_buf + CALIB_PAYLOAD_OFFSET,
                            (UINT32_t)sizeof(body));

    memcpy(image_buf + CALIB_HEADER_OFFSET, &hdr, sizeof(hdr));
}

calibration_status_t touch_calibration_save_to_storage(
        touch_calibration_t *p_calibration)
{
    if (NULL == p_calibration)
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    /**
     * The storage facade Write_CalibData triggers a full 4-KB sector erase
     * before programming, so writing only the header+body slice is safe —
     * every untouched byte in the sector will come back as 0xFF on the next
     * read, which is what the layout expects.  Keeping the buffer on the
     * stack (~40 B) avoids burning a static 4-KB block of RAM.
     */
    UINT8_t image_buf[CALIB_PAYLOAD_OFFSET +
                      sizeof(touch_calibration_persist_t)];
    pack_image(p_calibration, image_buf, (UINT32_t)sizeof(image_buf));

    const ext_flash_status_t st = Write_CalibData(CFG_TOUCH_CALIB_OFFSET,
                                                  (UINT32_t)sizeof(image_buf),
                                                  image_buf);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, TOUCH_CALIB_ERR_LOG_TAG,
                  "calib save: Write_CalibData failed st=%d", (int)st);
        return CALIBRATION_ERROR_STORAGE_FAILED;
    }

    DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG, "calib save: ok");
    return CALIBRATION_SUCCESS;
}

calibration_status_t touch_calibration_load_from_storage(
        touch_calibration_t *p_calibration)
{
    if (NULL == p_calibration)
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    /* Only need the first ~40 B but Read_CalibData accepts any size.  Read
     * header + payload in one go so we don't pay the storage round-trip
     * twice. */
    static UINT8_t s_load_buf[CALIB_PAYLOAD_OFFSET +
                              sizeof(touch_calibration_persist_t)];
    const ext_flash_status_t st = Read_CalibData(CFG_TOUCH_CALIB_OFFSET,
                                                 sizeof(s_load_buf),
                                                 s_load_buf);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, TOUCH_CALIB_ERR_LOG_TAG,
                  "calib load: Read_CalibData failed st=%d", (int)st);
        return CALIBRATION_ERROR_STORAGE_FAILED;
    }

    calibration_header_t hdr;
    memcpy(&hdr, s_load_buf + CALIB_HEADER_OFFSET, sizeof(hdr));

    if (CFG_TOUCH_CALIB_MAGIC != hdr.magic)
    {
        DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                  "calib load: magic mismatch (0x%08X)",
                  (unsigned)hdr.magic);
        return CALIBRATION_ERROR_MAGIC_MISMATCH;
    }
    if (CFG_TOUCH_CALIB_SCHEMA_VER != hdr.schema_version)
    {
        DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                  "calib load: schema mismatch (got %u want %u)",
                  (unsigned)hdr.schema_version,
                  (unsigned)CFG_TOUCH_CALIB_SCHEMA_VER);
        return CALIBRATION_ERROR_MAGIC_MISMATCH;
    }

    touch_calibration_persist_t body;
    memcpy(&body, s_load_buf + CALIB_PAYLOAD_OFFSET, sizeof(body));

    const UINT32_t crc_calc = calib_crc32(s_load_buf + CALIB_PAYLOAD_OFFSET,
                                          (UINT32_t)sizeof(body));
    if (crc_calc != hdr.crc32)
    {
        DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                  "calib load: crc mismatch (got 0x%08X want 0x%08X)",
                  (unsigned)hdr.crc32, (unsigned)crc_calc);
        return CALIBRATION_ERROR_CRC_MISMATCH;
    }
    if ((body.panel_w != CFG_TOUCH_PANEL_WIDTH) ||
        (body.panel_h != CFG_TOUCH_PANEL_HEIGHT))
    {
        DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                  "calib load: panel mismatch (saved %ux%u now %ux%u)",
                  (unsigned)body.panel_w, (unsigned)body.panel_h,
                  (unsigned)CFG_TOUCH_PANEL_WIDTH,
                  (unsigned)CFG_TOUCH_PANEL_HEIGHT);
        return CALIBRATION_ERROR_PANEL_MISMATCH;
    }

    p_calibration->matrix_a = body.matrix_a;
    p_calibration->matrix_b = body.matrix_b;
    p_calibration->matrix_c = body.matrix_c;
    p_calibration->matrix_d = body.matrix_d;
    p_calibration->matrix_e = body.matrix_e;
    p_calibration->matrix_f = body.matrix_f;
    p_calibration->panel_w  = body.panel_w;
    p_calibration->panel_h  = body.panel_h;
    p_calibration->is_calibrated = true;

    DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG,
              "calib load: a=%.4f b=%.4f c=%.2f d=%.4f e=%.4f f=%.2f",
              (DOUBLE)body.matrix_a, (DOUBLE)body.matrix_b,
              (DOUBLE)body.matrix_c, (DOUBLE)body.matrix_d,
              (DOUBLE)body.matrix_e, (DOUBLE)body.matrix_f);
    return CALIBRATION_SUCCESS;
}

touch_calibration_t *touch_calibration_get_instance(void)
{
    return &s_calibration;
}

BOOL touch_calibration_is_calibrated(void)
{
    return s_calibration.is_calibrated;
}
//******************************* Functions *********************************//
