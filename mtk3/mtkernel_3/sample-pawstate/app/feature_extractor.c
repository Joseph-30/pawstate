/**
 * @file feature_extractor.c
 * @brief Task 2 — Feature Extractor Implementation
 *
 * Computes 6 features from a 125-sample sliding window:
 *   1. Accel Variance — combined XYZ variance (motion intensity)
 *   2. Accel Mean Magnitude — average |a| (gravity + movement)
 *   3. Mag RMS Delta — magnetometer rate-of-change RMS (rotational proxy)
 *   4. Tilt Angle Delta — max pitch angle change over window
 *   5. Zero-Crossing Rate — directional change frequency (pacing detector)
 *   6. Activity Bout Duration — continuous motion time above threshold
 *
 * DESIGN DECISION on feature selection:
 * These 6 features were chosen because they are:
 *   - Computationally cheap (integer math only)
 *   - Orthogonal (each captures a different motion characteristic)
 *   - Clinically relevant (based on Ladha et al. 2013 veterinary research)
 *   - Discriminative for the 5 target classes
 */

#include "feature_extractor.h"
#include "imu_sampler.h"
#include "math_utils.h"
#include "circular_buffer.h"
#include "pawstate_config.h"

#include <tk/tkernel.h>
#include <string.h>
#include <tm/tmonitor.h>

/* Shared feature vector (protected by sem_feature_buf) */
static feature_vector_t current_features;

/* Local window buffer for feature computation */
static imu_sample_t window_buf[FEATURE_WINDOW_SIZE];

/* Temporary arrays for per-axis extraction */
static int16_t ax_buf[FEATURE_WINDOW_SIZE];
static int16_t ay_buf[FEATURE_WINDOW_SIZE];
static int16_t az_buf[FEATURE_WINDOW_SIZE];
static int16_t mag_delta_buf[FEATURE_WINDOW_SIZE];

/* External kernel object IDs */
extern ID flg_pipeline;
extern ID sem_feature_buf;

const feature_vector_t *feature_extractor_get_features(void)
{
    return &current_features;
}

/**
 * Decompose the window buffer into per-axis arrays and compute
 * magnetometer deltas (rate-of-change between consecutive samples).
 */
static void decompose_window(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        ax_buf[i] = window_buf[i].ax;
        ay_buf[i] = window_buf[i].ay;
        az_buf[i] = window_buf[i].az;
    }

    /* Compute magnetometer magnitude delta (consecutive differences).
     * This serves as a proxy for rotational velocity since the
     * micro:bit v2 has a magnetometer instead of a gyroscope.
     * Large deltas indicate the collar is rotating (head turning,
     * pacing direction changes). */
    mag_delta_buf[0] = 0;
    for (uint32_t i = 1; i < count; i++) {
        int32_t dmx = (int32_t)window_buf[i].mx - window_buf[i-1].mx;
        int32_t dmy = (int32_t)window_buf[i].my - window_buf[i-1].my;
        int32_t dmz = (int32_t)window_buf[i].mz - window_buf[i-1].mz;

        /* Approximate magnitude of delta vector */
        int32_t mag = isqrt((uint32_t)(dmx*dmx + dmy*dmy + dmz*dmz));
        /* Clamp to int16_t range */
        if (mag > 32767) mag = 32767;
        mag_delta_buf[i] = (int16_t)mag;
    }
}

/**
 * Compute Feature 1: Combined Acceleration Variance
 *
 * Sum of variance on each axis. High values indicate vigorous motion
 * (playing), medium values indicate locomotion (walking/pacing),
 * low values indicate stillness (resting/freeze).
 */
static q16_16_t compute_accel_variance(uint32_t count)
{
    q16_16_t mean_x = compute_mean_i16(ax_buf, count);
    q16_16_t mean_y = compute_mean_i16(ay_buf, count);
    q16_16_t mean_z = compute_mean_i16(az_buf, count);

    q16_16_t var_x = compute_variance_i16(ax_buf, count, mean_x);
    q16_16_t var_y = compute_variance_i16(ay_buf, count, mean_y);
    q16_16_t var_z = compute_variance_i16(az_buf, count, mean_z);

    /* Combined variance: sum of per-axis variances */
    return var_x + var_y + var_z;
}

/**
 * Compute Feature 2: Mean Acceleration Magnitude
 *
 * Average of |a| = sqrt(ax² + ay² + az²) over the window.
 * At rest, this equals ~1g (9.81 m/s²). Higher values indicate
 * additional motion-induced acceleration.
 */
static q16_16_t compute_accel_mean_mag(uint32_t count)
{
    int64_t sum = 0;

    for (uint32_t i = 0; i < count; i++) {
        q16_16_t mag = compute_magnitude_3d(ax_buf[i], ay_buf[i], az_buf[i]);
        sum += mag;
    }

    return (q16_16_t)(sum / (int64_t)count);
}

/**
 * Compute Feature 3: Magnetometer RMS Delta
 *
 * RMS of the inter-sample magnetometer magnitude changes.
 * Acts as a rotational velocity proxy. High values indicate
 * rapid head/body orientation changes (playing, pacing turns).
 */
static q16_16_t compute_mag_rms(uint32_t count)
{
    return compute_rms_i16(mag_delta_buf, count);
}

/**
 * Compute Feature 4: Tilt Angle Delta
 *
 * Maximum change in pitch angle (computed from accelerometer) over
 * the window. Large deltas indicate the dog is moving between
 * postures (standing ↔ lying, head up ↔ down).
 *
 * Pitch = atan2(ax, sqrt(ay² + az²))
 */
static q16_16_t compute_tilt_delta(uint32_t count)
{
    if (count < 2) return 0;

    q16_16_t min_pitch = 0x7FFFFFFF;
    q16_16_t max_pitch = (q16_16_t)0x80000001;

    for (uint32_t i = 0; i < count; i++) {
        /* Compute pitch angle from accelerometer */
        int32_t yz_sq = (int32_t)ay_buf[i] * ay_buf[i] +
                        (int32_t)az_buf[i] * az_buf[i];
        uint32_t yz_mag = isqrt((uint32_t)yz_sq);

        q16_16_t pitch = q16_atan2_deg(
            INT_TO_Q16((int32_t)ax_buf[i]),
            INT_TO_Q16((int32_t)yz_mag)
        );

        if (pitch < min_pitch) min_pitch = pitch;
        if (pitch > max_pitch) max_pitch = pitch;
    }

    /* Delta = max - min pitch over window */
    return max_pitch - min_pitch;
}

/**
 * Compute Feature 5: Zero-Crossing Rate
 *
 * Number of times the X-axis acceleration crosses zero (with hysteresis).
 * High rate → oscillating motion (pacing).
 * Medium rate → regular gait (walking).
 * Low rate → stationary or sustained motion (resting, freeze).
 *
 * DESIGN DECISION: Using X-axis (forward-backward in collar orientation)
 * because pacing produces the most distinctive zero-crossings on this axis.
 */
static q16_16_t compute_zcr(uint32_t count)
{
    return compute_zero_crossings(ax_buf, count, ZERO_CROSS_HYSTERESIS_MG);
}

/**
 * Compute Feature 6: Activity Bout Duration
 *
 * Total time (in ms, Q16.16) that the acceleration magnitude exceeds
 * MOTION_THRESHOLD_MG within the window. Distinguishes:
 *   - Continuous high activity (playing: long bout)
 *   - Intermittent activity (walking: medium bout)
 *   - Stationary (resting: zero bout)
 *   - Recent cessation (freeze: short/zero bout after activity)
 */
static q16_16_t compute_bout_duration(uint32_t count)
{
    uint32_t active_samples = 0;

    for (uint32_t i = 0; i < count; i++) {
        /* Check if acceleration exceeds motion threshold.
         * Compare magnitude to threshold. Use raw values scaled
         * appropriately: LSM303AGR at ±2g, 12-bit → 1mg ≈ 16 LSB */
        int32_t mag_sq = (int32_t)ax_buf[i] * ax_buf[i] +
                         (int32_t)ay_buf[i] * ay_buf[i] +
                         (int32_t)az_buf[i] * az_buf[i];

        /* Threshold squared to avoid sqrt:
         * threshold_mg = MOTION_THRESHOLD_MG
         * threshold_raw = threshold_mg * 16 (LSB per mg at ±2g HR mode)
         * threshold_sq = threshold_raw² */
        int32_t threshold_raw = MOTION_THRESHOLD_MG * 16;
        int64_t threshold_sq = (int64_t)threshold_raw * threshold_raw;

        /* Subtract gravity component (~1g = 16384 LSB at ±2g).
         * We compare against (mag - gravity)² but simplify by
         * checking if mag² differs significantly from gravity² */
        int64_t gravity_sq = (int64_t)16384 * 16384;
        int64_t deviation = (int64_t)mag_sq - gravity_sq;

        if (deviation > threshold_sq || deviation < -threshold_sq) {
            active_samples++;
        }
    }

    /* Convert active samples to milliseconds:
     * duration_ms = active_samples * (1000 / IMU_SAMPLE_RATE_HZ)
     *             = active_samples * 20 */
    return INT_TO_Q16((int32_t)(active_samples * (1000 / IMU_SAMPLE_RATE_HZ)));
}

/**
 * Feature Extractor Task — Priority 5
 *
 * Waits for EVT_NEW_SAMPLES from the IMU Sampler, reads a 125-sample
 * window, computes the 6-D feature vector, and signals the classifier.
 */
void feature_extractor_task(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    UINT flg_pattern;
    circular_buffer_t *cbuf = imu_sampler_get_buffer();

    for (;;) {
        /* Wait for the IMU sampler to signal a full window.
         * TWF_ORW: OR-wait, wake on any matching bit.
         * TWF_CLR: Clear the flag after waking. */
        tk_wai_flg(flg_pipeline, EVT_NEW_SAMPLES,
                   TWF_ORW | TWF_CLR, &flg_pattern, TMO_FEVR);

        /* Read the window from circular buffer (non-consuming peek) */
        uint32_t count = cbuf_peek_window(cbuf, window_buf,
                                          FEATURE_WINDOW_SIZE);

        if (count < FEATURE_WINDOW_SIZE / 2) {
            /* Not enough data — skip this cycle.
             * This can happen at startup before the buffer fills. */
            continue;
        }

        /* Consume the samples we've read */
        cbuf_consume(cbuf, count);

        /* Decompose window into per-axis arrays and compute mag deltas */
        decompose_window(count);

        /* Compute all 6 features */
        feature_vector_t fv;
        fv.accel_variance       = compute_accel_variance(count);
        fv.accel_mean_magnitude = compute_accel_mean_mag(count);
        fv.mag_rms_delta        = compute_mag_rms(count);
        fv.tilt_angle_delta     = compute_tilt_delta(count);
        fv.zero_crossing_rate   = compute_zcr(count);
        fv.activity_bout_dur    = compute_bout_duration(count);

        /* Write to shared feature buffer (protected by semaphore) */
        tk_wai_sem(sem_feature_buf, 1, TMO_FEVR);
        current_features = fv;
        tk_sig_sem(sem_feature_buf, 1);

        tm_printf((const UB*)"[FEAT] Extracted 6 features. Signaling Classifier...\n");

        /* Signal the classifier that features are ready */
        tk_set_flg(flg_pipeline, EVT_FEATURES_READY);
    }
}
