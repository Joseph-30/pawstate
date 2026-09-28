/**
 * @file pawstate_types.h
 * @brief PawState Shared Type Definitions
 *
 * All data structures shared across the PawState firmware modules.
 * Target: BBC micro:bit v2 (nRF52833) + μT-Kernel 3.0
 * Contest: TRON Programming Contest 2026
 */

#ifndef PAWSTATE_TYPES_H
#define PAWSTATE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "pawstate_config.h"

/* --- Behavioural State Enumeration ---
 * Five classes from veterinary literature (Ladha et al. 2013).
 * ANXIOUS_PACING and ALERT_FREEZE trigger high-priority BLE alerts. */
typedef enum {
    STATE_RESTING        = 0,
    STATE_WALKING        = 1,
    STATE_PLAYING        = 2,
    STATE_ANXIOUS_PACING = 3,
    STATE_ALERT_FREEZE   = 4,
    STATE_UNKNOWN        = 0xFF
} behaviour_state_t;

/* --- IMU Sample ---
 * Raw int16_t values from LSM303AGR. No float conversion at sample time
 * to minimise ISR duration. Accel: ±2g 12-bit left-justified, Mag: ±50 gauss 16-bit. */
typedef struct {
    int16_t ax, ay, az;  /* Accelerometer X/Y/Z */
    int16_t mx, my, mz;  /* Magnetometer  X/Y/Z */
} imu_sample_t;

/* --- Fixed-point Q16.16 --- */
typedef int32_t q16_16_t;

/* --- Feature Vector (6-D) ---
 * Computed from 125-sample sliding window. Q16.16 fixed-point. */
typedef struct {
    q16_16_t accel_variance;
    q16_16_t accel_mean_magnitude;
    q16_16_t mag_rms_delta;
    q16_16_t tilt_angle_delta;
    q16_16_t zero_crossing_rate;
    q16_16_t activity_bout_dur;
} feature_vector_t;

/* --- BLE Event (12 bytes, sent via message buffer) --- */
typedef struct {
    behaviour_state_t state;
    uint8_t           confidence;
    uint8_t           is_anxiety;
    uint8_t           reserved;
    uint32_t          timestamp_ms;
    behaviour_state_t prev_state;
    uint8_t           padding[3];
} ble_event_t;

/* --- Classifier Result --- */
typedef struct {
    behaviour_state_t predicted_class;
    uint8_t           confidence;
    uint8_t           class_probs[NUM_BEHAVIOUR_CLASSES];
    bool              is_anxiety_spike;
} classifier_result_t;

/* --- System Status --- */
typedef struct {
    behaviour_state_t current_state;
    uint8_t           confidence;
    bool              ble_connected;
    bool              imu_healthy;
    uint32_t          uptime_ms;
    uint32_t          sample_count;
    uint32_t          inference_count;
    int8_t            temperature_c;
} system_status_t;

/* --- Offline History Entry (4 bytes, for BLE-disconnected buffering) --- */
typedef struct {
    uint16_t          elapsed_sec;
    behaviour_state_t state;
    uint8_t           confidence;
} state_history_entry_t;

/* --- LED 5x5 Pattern --- */
typedef struct {
    uint8_t rows[5];  /* bit[4:0] → COL[5:1] */
} led_pattern_t;

#endif /* PAWSTATE_TYPES_H */
