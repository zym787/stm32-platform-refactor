/******************************************************************************
 * @file hr_algo_biquad.c
 *
 * @par dependencies
 * - hr_algo.h
 *
 * @author Ethan-Hang
 *
 * @brief Advanced PPG heart-rate algorithm: cascaded 2nd-order Butterworth
 *        high-pass + low-pass bandpass (0.5-5 Hz), adaptive envelope
 *        tracking, peak detection with
 *        adaptive threshold, IBI validation with coefficient-of-variation
 *        penalty on confidence.
 *
 *        The bandpass is implemented as a cascade of two 2nd-order biquad
 *        sections (Direct Form II transposed).  Coefficients are computed
 *        once during init via the bilinear transform.
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

#define PI_F                    (3.14159265358979f)

/** Minimum IBI count before BPM is reported. */
#define BIQUAD_MIN_IBI_FOR_BPM  (3U)

/** IBI outlier rejection fraction. */
#define BIQUAD_OUTLIER_FRAC     (0.3f)

/** Coefficient-of-variation threshold above which confidence is penalised. */
#define BIQUAD_CV_THRESHOLD     (0.15f)

/** Consecutive outliers before accepting a new rhythm as real. */
#define BIQUAD_OUTLIER_RESET_COUNT  (3U)

/** No accepted peak for this long means the previous BPM is stale. */
#define BIQUAD_SIGNAL_LOSS_MS       (HR_ALGO_IBI_MAX_MS * 2U)

/** Butterworth Q for a 2nd-order low-pass/high-pass section. */
#define BIQUAD_BUTTERWORTH_Q        (0.70710678118f)

//******************************** Defines **********************************//

//******************************* Functions *********************************//

/**
 * @brief      Compute a 2nd-order Butterworth low-pass section.
 *
 * @param[out] p_s        : Biquad section to populate.
 *
 * @param[in]  cutoff_hz : -3 dB cutoff frequency in Hz.
 *
 * @param[in]  fs         : Sample rate in Hz.
 */
static void calc_lowpass_coeffs(hr_biquad_section_t *p_s,
                                float cutoff_hz,
                                float fs)
{
    float omega = 2.0f * PI_F * cutoff_hz / fs;
    float sin_om = sinf(omega);
    float cos_om = cosf(omega);
    float alpha = sin_om / (2.0f * BIQUAD_BUTTERWORTH_Q);

    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_om;
    float a2 =  1.0f - alpha;

    float b0 = (1.0f - cos_om) * 0.5f;
    float b1 =  1.0f - cos_om;
    float b2 = (1.0f - cos_om) * 0.5f;

    p_s->b0 = b0 / a0;
    p_s->b1 = b1 / a0;
    p_s->b2 = b2 / a0;
    p_s->a1 = a1 / a0;
    p_s->a2 = a2 / a0;
    p_s->z1 = 0.0f;
    p_s->z2 = 0.0f;
}

/**
 * @brief      Compute a 2nd-order Butterworth high-pass section.
 */
static void calc_highpass_coeffs(hr_biquad_section_t *p_s,
                                 float cutoff_hz,
                                 float fs)
{
    float omega = 2.0f * PI_F * cutoff_hz / fs;
    float sin_om = sinf(omega);
    float cos_om = cosf(omega);
    float alpha = sin_om / (2.0f * BIQUAD_BUTTERWORTH_Q);

    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_om;
    float a2 =  1.0f - alpha;

    float b0 = (1.0f + cos_om) * 0.5f;
    float b1 = -(1.0f + cos_om);
    float b2 = (1.0f + cos_om) * 0.5f;

    p_s->b0 = b0 / a0;
    p_s->b1 = b1 / a0;
    p_s->b2 = b2 / a0;
    p_s->a1 = a1 / a0;
    p_s->a2 = a2 / a0;
    p_s->z1 = 0.0f;
    p_s->z2 = 0.0f;
}

/**
 * @brief      Process one sample through a single biquad section
 *             (Direct Form II transposed).
 *
 * @param[in,out] p_s     : Biquad section with state.
 *
 * @param[in]     input   : Input sample.
 *
 * @return     Filtered output sample.
 */
static float biquad_process(hr_biquad_section_t *p_s, float input)
{
    float out = p_s->b0 * input + p_s->z1;
    p_s->z1   = p_s->b1 * input - p_s->a1 * out + p_s->z2;
    p_s->z2   = p_s->b2 * input - p_s->a2 * out;
    return out;
}

/**
 * @brief      Compute the mean of a uint32_t array.
 */
static uint32_t mean_uint32(const uint32_t *buf, uint8_t len)
{
    uint32_t sum = 0U;
    for (uint8_t i = 0U; i < len; i++)
    {
        sum += buf[i];
    }
    return sum / (uint32_t)len;
}

/**
 * @brief      Compute the median of a uint32_t array via insertion sort.
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
 * @brief      Compute the standard deviation of a uint32_t array.
 */
static float stddev_uint32(const uint32_t *buf, uint8_t len)
{
    if (len < 2U)
    {
        return 0.0f;
    }

    float mean = (float)mean_uint32(buf, len);
    float sum_sq = 0.0f;

    for (uint8_t i = 0U; i < len; i++)
    {
        float d = (float)buf[i] - mean;
        sum_sq += d * d;
    }

    return sqrtf(sum_sq / (float)(len - 1U));
}

/**
 * @brief      Check whether an IBI is an outlier.
 */
static bool ibi_is_outlier(const hr_algo_state_t *p_state, uint32_t ibi_ms)
{
    if (p_state->ibi_count < BIQUAD_MIN_IBI_FOR_BPM)
    {
        return false;
    }

    uint32_t med = median_uint32(p_state->ibi_buf, p_state->ibi_count);
    float    dev = fabsf((float)(int32_t)ibi_ms - (float)(int32_t)med);

    return (dev > BIQUAD_OUTLIER_FRAC * (float)med);
}

/**
 * @brief      Validate the biquad algorithm configuration.
 */
static bool biquad_cfg_is_valid(hr_algo_biquad_cfg_t const *p_cfg)
{
    if (p_cfg == NULL)
    {
        return false;
    }

    if (!((p_cfg->sample_rate_hz > 0.0f) &&
          (p_cfg->freq_low_hz > 0.0f) &&
          (p_cfg->freq_high_hz > p_cfg->freq_low_hz) &&
          (p_cfg->freq_high_hz < (p_cfg->sample_rate_hz * 0.5f))))
    {
        return false;
    }
    if (!((p_cfg->adaptive_env_alpha > 0.0f) &&
          (p_cfg->adaptive_env_alpha <= 1.0f)))
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
    if ((p_cfg->ibi_window_size < BIQUAD_MIN_IBI_FOR_BPM) ||
        (p_cfg->ibi_window_size > HR_ALGO_IBI_WINDOW_MAX))
    {
        return false;
    }

    return true;
}

/**
 * @brief      Clear beat history while preserving filter/config state.
 */
static void biquad_clear_beat_history(hr_algo_state_t *p_state)
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
static void biquad_push_ibi(hr_algo_state_t *p_state, uint32_t interval_ms)
{
    hr_algo_biquad_cfg_t const *cfg = &p_state->inner.biquad.cfg;
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

/* ---- Biquad algorithm -------------------------------------------------- */

/**
 * @brief      Initialize the biquad algorithm.
 */
int32_t hr_algo_biquad_init_inner(hr_algo_state_t            *p_state,
                                  hr_algo_biquad_cfg_t const *p_cfg)
{
    if ((p_state == NULL) || (!biquad_cfg_is_valid(p_cfg)))
    {
        return -1;
    }

    memset(p_state, 0, sizeof(*p_state));
    p_state->type             = HR_ALGO_BIQUAD;
    p_state->first_peak       = true;
    p_state->inner.biquad.cfg = *p_cfg;

    /* Section 0 removes baseline wander; section 1 removes high-frequency
     * noise.  Together they form a 4th-order bandpass for the PPG band. */
    calc_highpass_coeffs(&p_state->inner.biquad.bp[0],
                         p_cfg->freq_low_hz,
                         p_cfg->sample_rate_hz);
    calc_lowpass_coeffs(&p_state->inner.biquad.bp[1],
                        p_cfg->freq_high_hz,
                        p_cfg->sample_rate_hz);

    return 0;
}

/**
 * @brief      Process one PPG sample through the biquad algorithm.
 */
int32_t hr_algo_biquad_process_inner(hr_algo_state_t    *p_state,
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

    hr_algo_biquad_cfg_t *cfg = &p_state->inner.biquad.cfg;

    if ((!p_state->first_peak) &&
        ((timestamp_ms - p_state->last_peak_ts_ms) > BIQUAD_SIGNAL_LOSS_MS))
    {
        biquad_clear_beat_history(p_state);
    }

    /* 1. Cascade of two biquad sections (4th-order bandpass). */
    float filtered = (float)sample;
    filtered = biquad_process(&p_state->inner.biquad.bp[0], filtered);
    filtered = biquad_process(&p_state->inner.biquad.bp[1], filtered);

    /* 2. Adaptive envelope tracking. */
    float abs_val = fabsf(filtered);
    p_state->inner.biquad.envelope =
        cfg->adaptive_env_alpha * abs_val
        + (1.0f - cfg->adaptive_env_alpha) * p_state->inner.biquad.envelope;

    /* 3. Peak detection (same state machine, adaptive threshold). */
    if (filtered > p_state->prev_signal)
    {
        p_state->rising = true;
        if (filtered > p_state->peak_value)
        {
            p_state->peak_value = filtered;
        }
    }
    else if (p_state->rising)
    {
        p_state->rising = false;
        p_state->total_peaks++;

        float threshold = p_state->inner.biquad.envelope
                        * cfg->peak_threshold_frac;

        /* Enforce a minimum absolute threshold to avoid noise triggers. */
        if (threshold < 1.0f)
        {
            threshold = 1.0f;
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
                            biquad_push_ibi(p_state, interval);
                            p_result->beat_detected = true;
                        }
                        else
                        {
                            p_state->outlier_streak++;
                            if (p_state->outlier_streak >=
                                BIQUAD_OUTLIER_RESET_COUNT)
                            {
                                biquad_clear_beat_history(p_state);
                                p_state->first_peak = false;
                                p_state->total_peaks = 1U;
                                biquad_push_ibi(p_state, interval);
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

    p_state->prev_signal = filtered;

    /* 4. BPM from IBI window median. */
    if (p_state->ibi_count >= BIQUAD_MIN_IBI_FOR_BPM)
    {
        uint32_t med = median_uint32(p_state->ibi_buf, p_state->ibi_count);
        if (med > 0U)
        {
            p_result->bpm = (uint16_t)(60000.0f / (float)med + 0.5f);
        }
    }

    /* 5. Confidence with CV penalty. */
    if (p_state->total_peaks > 0U)
    {
        uint32_t conf = (p_state->valid_peaks * 100U) / p_state->total_peaks;

        /* Penalise if IBI variance is high. */
        if (p_state->ibi_count >= BIQUAD_MIN_IBI_FOR_BPM)
        {
            float mean = (float)mean_uint32(p_state->ibi_buf,
                                            p_state->ibi_count);
            if (mean > 0.0f)
            {
                float sd  = stddev_uint32(p_state->ibi_buf,
                                          p_state->ibi_count);
                float cv  = sd / mean;
                if (cv > BIQUAD_CV_THRESHOLD)
                {
                    float penalty = cv;
                    if (penalty > 0.5f)
                    {
                        penalty = 0.5f;
                    }
                    conf = (uint32_t)((float)conf * (1.0f - penalty));
                }
            }
        }

        p_result->confidence = (uint8_t)conf;
    }

    return 0;
}

/**
 * @brief      Reset the biquad algorithm state (preserve config).
 */
void hr_algo_biquad_reset_inner(hr_algo_state_t *p_state)
{
    if (p_state == NULL)
    {
        return;
    }

    hr_algo_biquad_cfg_t saved = p_state->inner.biquad.cfg;
    memset(p_state, 0, sizeof(*p_state));
    p_state->type              = HR_ALGO_BIQUAD;
    p_state->first_peak        = true;
    p_state->inner.biquad.cfg  = saved;

    /* Recompute coefficients (state is zeroed). */
    calc_highpass_coeffs(&p_state->inner.biquad.bp[0],
                         saved.freq_low_hz,
                         saved.sample_rate_hz);
    calc_lowpass_coeffs(&p_state->inner.biquad.bp[1],
                        saved.freq_high_hz,
                        saved.sample_rate_hz);
}

//******************************* Functions *********************************//
