/******************************************************************************
 * @file hr_algo.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - stddef.h
 *
 * @author Ethan-Hang
 *
 * @brief PPG heart-rate calculation algorithm public interface.
 *
 *        Pure DSP module -- no OSAL, BSP, or hardware dependency.
 *        Two algorithm variants selectable at init time:
 *          HR_ALGO_SIMPLE : EMA high-pass + smoothing + peak detection
 *          HR_ALGO_BIQUAD : cascaded Butterworth bandpass + adaptive threshold
 *
 *        Caller feeds one raw PPG sample (hrs1_sum) per call and receives
 *        a result struct containing BPM, confidence, and beat flag.
 *
 * @version V1.0 2026-05-10
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __HR_ALGO_H__
#define __HR_ALGO_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/** Maximum IBI ring-buffer depth. */
#define HR_ALGO_IBI_WINDOW_MAX      (8U)

/** Minimum valid IBI in ms (240 BPM ceiling). */
#define HR_ALGO_IBI_MIN_MS          (250U)

/** Maximum valid IBI in ms (40 BPM floor). */
#define HR_ALGO_IBI_MAX_MS          (1500U)

/**
 * @brief Algorithm selector.
 */
typedef enum
{
    HR_ALGO_SIMPLE = 0,       /* EMA high-pass + smoothing + peak detect       */
    HR_ALGO_BIQUAD = 1,       /* 4th-order Butterworth bandpass + adaptive peak */
} hr_algo_type_t;

/**
 * @brief Algorithm result -- produced on every hr_algo_process() call.
 */
typedef struct
{
    uint16_t bpm;             /* Beats per minute; 0 = not yet available       */
    uint8_t  confidence;      /* 0-100, ratio of valid peaks to total detected */
    uint32_t timestamp_ms;    /* Timestamp of the triggering sample            */
    bool     beat_detected;   /* true if this sample triggered a new beat      */
} hr_algo_result_t;

/**
 * @brief Configuration for the simple (EMA + peak detect) algorithm.
 */
typedef struct
{
    float    alpha_dc;                /* DC removal EMA coefficient,      default 0.01f  */
    float    alpha_sm;                /* Smoothing EMA coefficient,       default 0.2f   */
    float    peak_threshold_frac;     /* Peak amplitude threshold as a fraction
                                       * of recent max,                   default 0.4f   */
    uint32_t min_peak_interval_ms;    /* Min time between peaks,          default 250    */
    uint8_t  ibi_window_size;         /* Number of IBIs in sliding window,default 8      */
} hr_algo_simple_cfg_t;

/**
 * @brief Configuration for the biquad (bandpass + adaptive) algorithm.
 */
typedef struct
{
    float    freq_low_hz;             /* Bandpass lower -3 dB,            default 0.5f   */
    float    freq_high_hz;            /* Bandpass upper -3 dB,            default 5.0f   */
    float    sample_rate_hz;          /* Sample rate,                     default 40.0f  */
    float    adaptive_env_alpha;      /* Envelope tracking EMA,           default 0.05f  */
    float    peak_threshold_frac;     /* Adaptive threshold fraction,     default 0.35f  */
    uint32_t min_peak_interval_ms;    /* Min time between peaks,          default 250    */
    uint8_t  ibi_window_size;         /* Number of IBIs in sliding window,default 8      */
} hr_algo_biquad_cfg_t;

/**
 * @brief Combined configuration, selected by the type field.
 */
typedef struct
{
    hr_algo_type_t type;
    union
    {
        hr_algo_simple_cfg_t simple;
        hr_algo_biquad_cfg_t biquad;
    } cfg;
} hr_algo_config_t;

/**
 * @brief One biquad filter section (Direct Form II transposed).
 *        Used internally by the BIQUAD algorithm variant.
 */
typedef struct
{
    float b0, b1, b2;       /* numerator coefficients          */
    float a1, a2;           /* denominator (a0 = 1)            */
    float z1, z2;           /* filter state                    */
} hr_biquad_section_t;

/**
 * @brief Algorithm state -- allocated by the caller on the stack.
 *
 *        The type field selects which branch of the inner union is valid.
 *        Both algorithm implementations use the same struct layout; the
 *        dispatch layer reads only the type field to forward calls.
 */
typedef struct hr_algo_state_s
{
    hr_algo_type_t type;

    /* ---- Common peak detection state (both variants) ---- */
    bool     rising;
    float    prev_signal;
    float    peak_value;
    uint32_t last_peak_ts_ms;
    bool     first_peak;

    /* ---- IBI ring buffer ---- */
    uint32_t ibi_buf[HR_ALGO_IBI_WINDOW_MAX];
    uint8_t  ibi_idx;
    uint8_t  ibi_count;
    uint8_t  outlier_streak;

    /* ---- Statistics ---- */
    uint32_t total_peaks;
    uint32_t valid_peaks;

    /* ---- Algorithm-specific state ---- */
    union
    {
        /* SIMPLE variant */
        struct
        {
            float dc;               /* DC estimate for high-pass EMA       */
            float smooth;           /* Low-pass smoothed signal            */
            float recent_max;       /* Running max amplitude for threshold  */
            hr_algo_simple_cfg_t cfg;
        } simple;

        /* BIQUAD variant */
        struct
        {
            hr_biquad_section_t bp[2];  /* Cascade of 2 biquad sections    */
            float               envelope; /* Adaptive signal envelope        */
            hr_algo_biquad_cfg_t cfg;
        } biquad;
    } inner;

} hr_algo_state_t;

//******************************** Defines **********************************//

//******************************* Functions *********************************//

/**
 * @brief      Return the default configuration for the given algorithm type.
 *
 * @param[in]  type  : Algorithm type (SIMPLE or BIQUAD).
 *
 * @param[out] p_cfg : Filled with defaults.
 *
 * @return     0 on success, -1 on invalid parameter.
 */
int32_t hr_algo_get_default_config(hr_algo_type_t  type,
                                   hr_algo_config_t *p_cfg);

/**
 * @brief      Initialize algorithm state with the given configuration.
 *
 * @param[out] p_state : Pointer to caller-allocated state struct.
 *
 * @param[in]  p_cfg   : Algorithm configuration (type + parameters).
 *
 * @return     0 on success, -1 on invalid parameter.
 */
int32_t hr_algo_init(hr_algo_state_t          *p_state,
                     hr_algo_config_t   const *p_cfg);

/**
 * @brief      Feed one raw PPG sample and receive the latest result.
 *
 * @param[in,out] p_state     : Algorithm state.
 *
 * @param[in]     sample      : Raw PPG sample (typically hrs1_sum).
 *
 * @param[in]     timestamp_ms: Frame timestamp in milliseconds.
 *
 * @param[out]    p_result    : Output result (bpm, confidence, beat flag).
 *
 * @return     0 on success, -1 on invalid parameter.
 */
int32_t hr_algo_process(hr_algo_state_t    *p_state,
                        uint32_t            sample,
                        uint32_t            timestamp_ms,
                        hr_algo_result_t   *p_result);

/**
 * @brief      Reset algorithm state (clear filters, IBIs, peak history).
 *              Configuration is preserved.
 *
 * @param[in,out] p_state : Algorithm state.
 *
 * @return     0 on success, -1 on invalid parameter.
 */
int32_t hr_algo_reset(hr_algo_state_t *p_state);

//******************************* Functions *********************************//

#endif /* __HR_ALGO_H__ */
