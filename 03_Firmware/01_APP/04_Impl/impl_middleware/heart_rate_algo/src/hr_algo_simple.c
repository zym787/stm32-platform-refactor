/******************************************************************************
 * @file hr_algo_simple.c
 *
 * @par dependencies
 * - hr_algo.h
 *
 * @author Ethan-Hang
 *
 * @brief Simple PPG heart-rate algorithm: EMA high-pass DC removal,
 *        EMA smoothing, peak detection state machine, IBI validation
 *        with outlier rejection, median-based BPM calculation.
 *
 *        Also implements the unified hr_algo_init / hr_algo_process /
 *        hr_algo_reset dispatch that routes to the correct variant.
 *
 * @version V1.0 2026-05-10
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "hr_algo.h"

#include <string.h>
#include <math.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/** Slow-decay factor for the running-max envelope. */
#define SIMPLE_MAX_DECAY_FACTOR     (0.999f)

/** IBI outlier rejection: reject if deviation > 30% of window median. */
#define SIMPLE_OUTLIER_FRAC         (0.3f)

/** Minimum IBI count before BPM is reported. */
#define SIMPLE_MIN_IBI_FOR_BPM      (3U)

/** Consecutive outliers before accepting a new rhythm as real. */
#define SIMPLE_OUTLIER_RESET_COUNT  (3U)

/** No accepted peak for this long means the previous BPM is stale. */
#define SIMPLE_SIGNAL_LOSS_MS       (HR_ALGO_IBI_MAX_MS * 2U)

/** Minimum threshold to prevent zero-envelope noise from becoming a beat. */
#define SIMPLE_MIN_PEAK_THRESHOLD   (1.0f)

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

/* Forward declarations for biquad functions (defined in hr_algo_biquad.c).
 * When biquad.c is NOT linked, these are unresolved -- the BIQUAD dispatch
 * path will fail at link time, which is the intended behaviour. */
int32_t hr_algo_biquad_init_inner(hr_algo_state_t          *p_state,
                                  hr_algo_biquad_cfg_t const *p_cfg);
int32_t hr_algo_biquad_process_inner(hr_algo_state_t    *p_state,
                                     uint32_t            sample,
                                     uint32_t            timestamp_ms,
                                     hr_algo_result_t   *p_result);
void    hr_algo_biquad_reset_inner(hr_algo_state_t *p_state);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief      Compute the median of a uint32_t array via insertion sort.
 *
 * @param[in]  buf : Array of values.
 *
 * @param[in]  len : Number of valid elements (1 .. HR_ALGO_IBI_WINDOW_MAX).
 *
 * @return     Median value.
 */
static uint32_t median_uint32(const uint32_t *buf, uint8_t len)
{
    uint32_t tmp[HR_ALGO_IBI_WINDOW_MAX];
    memcpy(tmp, buf, (size_t)len * sizeof(uint32_t));

    for (uint8_t i = 1U; i < len; i++)
    {
        uint32_t key = tmp[i];
        uint8_t  j   = i;
        while ((j > 0U) && (tmp[j - 1U] > key))
        {
            tmp[j] = tmp[j - 1U];
            j--;
        }
        tmp[j] = key;
    }

    return tmp[len / 2U];
}

/**
 * @brief      Check whether an IBI is an outlier relative to the window.
 */
static bool ibi_is_outlier(const hr_algo_state_t *p_state, uint32_t ibi_ms)
{
    if (p_state->ibi_count < SIMPLE_MIN_IBI_FOR_BPM)
    {
        return false;
    }

    uint32_t med = median_uint32(p_state->ibi_buf, p_state->ibi_count);
    float    dev = fabsf((float)(int32_t)ibi_ms - (float)(int32_t)med);

    return (dev > SIMPLE_OUTLIER_FRAC * (float)med);
}

/**
 * @brief      Validate the simple algorithm configuration.
 */
static bool simple_cfg_is_valid(hr_algo_simple_cfg_t const *p_cfg)
{
    if (p_cfg == NULL)
    {
        return false;
    }

    if (!((p_cfg->alpha_dc > 0.0f) && (p_cfg->alpha_dc <= 1.0f)))
    {
        return false;
    }
    if (!((p_cfg->alpha_sm > 0.0f) && (p_cfg->alpha_sm <= 1.0f)))
    {
        return false;
    }
    if (!((p_cfg->peak_threshold_frac > 0.0f) &&
          (p_cfg->peak_threshold_frac <= 1.0f)))
    {
        return false;
    }
    if ((p_cfg->min_peak_interval_ms < HR_ALGO_IBI_MIN_MS) ||
        (p_cfg->min_peak_interval_ms > HR_ALGO_IBI_MAX_MS))
    {
        return false;
    }
    if ((p_cfg->ibi_window_size < SIMPLE_MIN_IBI_FOR_BPM) ||
        (p_cfg->ibi_window_size > HR_ALGO_IBI_WINDOW_MAX))
    {
        return false;
    }

    return true;
}

/**
 * @brief      Clear beat history while preserving filter/config state.
 */
static void simple_clear_beat_history(hr_algo_state_t *p_state)
{
    memset(p_state->ibi_buf, 0, sizeof(p_state->ibi_buf));
    p_state->ibi_idx          = 0U;
    p_state->ibi_count        = 0U;
    p_state->outlier_streak   = 0U;
    p_state->total_peaks      = 0U;
    p_state->valid_peaks      = 0U;
    p_state->first_peak       = true;
    p_state->last_peak_ts_ms  = 0U;
    p_state->rising           = false;
    p_state->peak_value       = 0.0f;
}

/**
 * @brief      Append one accepted IBI into the sliding window.
 */
static void simple_push_ibi(hr_algo_state_t *p_state, uint32_t interval_ms)
{
    hr_algo_simple_cfg_t const *cfg = &p_state->inner.simple.cfg;
    uint8_t idx = p_state->ibi_idx;

    p_state->ibi_buf[idx] = interval_ms;
    p_state->ibi_idx = (uint8_t)((idx + 1U) % cfg->ibi_window_size);
    if (p_state->ibi_count < cfg->ibi_window_size)
    {
        p_state->ibi_count++;
    }
    p_state->valid_peaks++;
    p_state->outlier_streak = 0U;
}

/* ---- Simple algorithm -------------------------------------------------- */

/**
 * @brief      Initialize the simple algorithm.
 */
static int32_t hr_algo_simple_init(hr_algo_state_t            *p_state,
                                   hr_algo_simple_cfg_t const *p_cfg)
{
    if ((p_state == NULL) || (!simple_cfg_is_valid(p_cfg)))
    {
        return -1;
    }

    memset(p_state, 0, sizeof(*p_state));
    p_state->type        = HR_ALGO_SIMPLE;
    p_state->first_peak  = true;
    p_state->inner.simple.cfg = *p_cfg;

    return 0;
}

/**
 * @brief      Process one PPG sample through the simple algorithm.
 */
static int32_t hr_algo_simple_process(hr_algo_state_t    *p_state,
                                      uint32_t            sample,
                                      uint32_t            timestamp_ms,
                                      hr_algo_result_t   *p_result)
{
    if ((p_state == NULL) || (p_result == NULL))
    {
        return -1;
    }

    /* Default output. */
    p_result->bpm           = 0U;
    p_result->confidence    = 0U;
    p_result->timestamp_ms  = timestamp_ms;
    p_result->beat_detected = false;

    hr_algo_simple_cfg_t *cfg = &p_state->inner.simple.cfg;
    float raw = (float)sample;

    if ((!p_state->first_peak) &&
        ((timestamp_ms - p_state->last_peak_ts_ms) > SIMPLE_SIGNAL_LOSS_MS))
    {
        simple_clear_beat_history(p_state);
    }

    /* 1. DC removal (high-pass EMA). */
    p_state->inner.simple.dc = cfg->alpha_dc * raw
        + (1.0f - cfg->alpha_dc) * p_state->inner.simple.dc;
    float signal = raw - p_state->inner.simple.dc;

    /* 2. Smoothing (low-pass EMA). */
    p_state->inner.simple.smooth = cfg->alpha_sm * signal
        + (1.0f - cfg->alpha_sm) * p_state->inner.simple.smooth;

    /* 3. Update running max with slow decay. */
    if (p_state->inner.simple.smooth > p_state->inner.simple.recent_max)
    {
        p_state->inner.simple.recent_max = p_state->inner.simple.smooth;
    }
    else
    {
        p_state->inner.simple.recent_max *= SIMPLE_MAX_DECAY_FACTOR;
    }

    float smooth = p_state->inner.simple.smooth;

    /* 4. Peak detection state machine. */
    if (smooth > p_state->prev_signal)
    {
        p_state->rising = true;
        if (smooth > p_state->peak_value)
        {
            p_state->peak_value = smooth;
        }
    }
    else if (p_state->rising)
    {
        /* Rising -> falling transition: candidate peak. */
        p_state->rising = false;
        p_state->total_peaks++;

        float threshold = p_state->inner.simple.recent_max
                        * cfg->peak_threshold_frac;
        if (threshold < SIMPLE_MIN_PEAK_THRESHOLD)
        {
            threshold = SIMPLE_MIN_PEAK_THRESHOLD;
        }

        if (p_state->peak_value > threshold)
        {
            if (p_state->first_peak)
            {
                p_state->last_peak_ts_ms = timestamp_ms;
                p_state->first_peak      = false;
                p_result->beat_detected  = true;
            }
            else
            {
                uint32_t interval = timestamp_ms - p_state->last_peak_ts_ms;

                if (interval >= cfg->min_peak_interval_ms)
                {
                    if ((interval >= HR_ALGO_IBI_MIN_MS) &&
                        (interval <= HR_ALGO_IBI_MAX_MS))
                    {
                        if (!ibi_is_outlier(p_state, interval))
                        {
                            simple_push_ibi(p_state, interval);
                            p_result->beat_detected = true;
                        }
                        else
                        {
                            p_state->outlier_streak++;
                            if (p_state->outlier_streak >=
                                SIMPLE_OUTLIER_RESET_COUNT)
                            {
                                simple_clear_beat_history(p_state);
                                p_state->first_peak = false;
                                p_state->total_peaks = 1U;
                                simple_push_ibi(p_state, interval);
                                p_result->beat_detected = true;
                            }
                        }
                    }
                    p_state->last_peak_ts_ms = timestamp_ms;
                }
            }
        }

        p_state->peak_value = 0.0f;
    }

    p_state->prev_signal = smooth;

    /* 5. BPM from IBI window median. */
    if (p_state->ibi_count >= SIMPLE_MIN_IBI_FOR_BPM)
    {
        uint32_t med = median_uint32(p_state->ibi_buf, p_state->ibi_count);
        if (med > 0U)
        {
            p_result->bpm = (uint16_t)(60000.0f / (float)med + 0.5f);
        }
    }

    /* 6. Confidence. */
    if (p_state->total_peaks > 0U)
    {
        p_result->confidence = (uint8_t)((p_state->valid_peaks * 100U)
                                        / p_state->total_peaks);
    }

    return 0;
}

/**
 * @brief      Reset the simple algorithm state (preserve config).
 */
static void hr_algo_simple_reset(hr_algo_state_t *p_state)
{
    if (p_state == NULL)
    {
        return;
    }

    hr_algo_simple_cfg_t saved = p_state->inner.simple.cfg;
    memset(p_state, 0, sizeof(*p_state));
    p_state->type              = HR_ALGO_SIMPLE;
    p_state->first_peak        = true;
    p_state->inner.simple.cfg  = saved;
}

/* ---- Unified dispatch --------------------------------------------------- */

/**
 * @brief      Return default configuration for the given algorithm type.
 */
int32_t hr_algo_get_default_config(hr_algo_type_t    type,
                                   hr_algo_config_t *p_cfg)
{
    if (p_cfg == NULL)
    {
        return -1;
    }

    memset(p_cfg, 0, sizeof(*p_cfg));
    p_cfg->type = type;

    switch (type)
    {
    case HR_ALGO_SIMPLE:
        p_cfg->cfg.simple.alpha_dc             = 0.01f;
        p_cfg->cfg.simple.alpha_sm             = 0.2f;
        p_cfg->cfg.simple.peak_threshold_frac  = 0.4f;
        p_cfg->cfg.simple.min_peak_interval_ms = 250U;
        p_cfg->cfg.simple.ibi_window_size      = 8U;
        break;

    case HR_ALGO_BIQUAD:
        p_cfg->cfg.biquad.freq_low_hz          = 0.5f;
        p_cfg->cfg.biquad.freq_high_hz         = 5.0f;
        p_cfg->cfg.biquad.sample_rate_hz       = 40.0f;
        p_cfg->cfg.biquad.adaptive_env_alpha   = 0.05f;
        p_cfg->cfg.biquad.peak_threshold_frac  = 0.35f;
        p_cfg->cfg.biquad.min_peak_interval_ms = 250U;
        p_cfg->cfg.biquad.ibi_window_size      = 8U;
        break;

    default:
        return -1;
    }

    return 0;
}

/**
 * @brief      Unified init -- routes to the correct variant.
 */
int32_t hr_algo_init(hr_algo_state_t        *p_state,
                     hr_algo_config_t const *p_cfg)
{
    if ((p_state == NULL) || (p_cfg == NULL))
    {
        return -1;
    }

    switch (p_cfg->type)
    {
    case HR_ALGO_SIMPLE:
        return hr_algo_simple_init(p_state, &p_cfg->cfg.simple);

    case HR_ALGO_BIQUAD:
        return hr_algo_biquad_init_inner(p_state, &p_cfg->cfg.biquad);

    default:
        return -1;
    }
}

/**
 * @brief      Unified process -- routes to the correct variant.
 */
int32_t hr_algo_process(hr_algo_state_t    *p_state,
                        uint32_t            sample,
                        uint32_t            timestamp_ms,
                        hr_algo_result_t   *p_result)
{
    if ((p_state == NULL) || (p_result == NULL))
    {
        return -1;
    }

    switch (p_state->type)
    {
    case HR_ALGO_SIMPLE:
        return hr_algo_simple_process(p_state, sample, timestamp_ms, p_result);

    case HR_ALGO_BIQUAD:
        return hr_algo_biquad_process_inner(p_state, sample,
                                            timestamp_ms, p_result);

    default:
        return -1;
    }
}

/**
 * @brief      Unified reset -- routes to the correct variant.
 */
int32_t hr_algo_reset(hr_algo_state_t *p_state)
{
    if (p_state == NULL)
    {
        return -1;
    }

    switch (p_state->type)
    {
    case HR_ALGO_SIMPLE:
        hr_algo_simple_reset(p_state);
        return 0;

    case HR_ALGO_BIQUAD:
        hr_algo_biquad_reset_inner(p_state);
        return 0;

    default:
        return -1;
    }
}

//******************************* Functions *********************************//
