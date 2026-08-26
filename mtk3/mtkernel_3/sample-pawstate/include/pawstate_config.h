/**
 * @file pawstate_config.h
 * @brief PawState Global Configuration Constants
 *
 * Central configuration for the PawState real-time canine emotion and
 * activity monitoring system. All tuneable parameters are defined here
 * to allow single-point modification without touching application logic.
 *
 * Target: BBC micro:bit v2 (nRF52833) + μT-Kernel 3.0
 * Contest: TRON Programming Contest 2026
 *
 * DESIGN DECISION: All timing constants are expressed in milliseconds
 * and converted to μT-Kernel tick units at compile time. μT-Kernel 3.0
 * default tick is 1ms (configurable via TK_TIMER_PERIOD).
 */

#ifndef PAWSTATE_CONFIG_H
#define PAWSTATE_CONFIG_H

/* =========================================================================
 * System Clock & Timer Configuration
 * ========================================================================= */

/** nRF52833 system clock frequency (Hz) */
#define SYS_CLOCK_HZ               64000000UL

/** μT-Kernel timer tick period (ms) — must match kernel config */
#define TK_TIMER_PERIOD_MS          1

/* =========================================================================
 * IMU Sampling Configuration
 * ========================================================================= */

/** IMU sampling frequency in Hz.
 *  DESIGN DECISION: 50 Hz provides sufficient temporal resolution for
 *  canine gait cycle detection (~2 Hz walking cadence) while staying
 *  well within the LSM303AGR's max ODR of 400 Hz.
 */
#define IMU_SAMPLE_RATE_HZ          50

/** IMU sampling period in milliseconds (1000 / 50 = 20ms) */
#define IMU_SAMPLE_PERIOD_MS        (1000 / IMU_SAMPLE_RATE_HZ)

/** Number of axes sampled per reading (3 accel + 3 magnetometer) */
#define IMU_AXES_PER_SAMPLE         6

/* =========================================================================
 * Feature Extraction Configuration
 * ========================================================================= */

/** Sliding window size in samples.
 *  DESIGN DECISION: 125 samples at 50 Hz = 2.5 second window.
 *  This captures at least one full walking gait cycle (~1.5s for medium
 *  breeds) and provides enough temporal context for anxiety pacing
 *  detection (rhythmic oscillation period ~1-3s).
 */
#define FEATURE_WINDOW_SIZE         125

/** Feature extraction period in milliseconds (matches window duration) */
#define FEATURE_EXTRACT_PERIOD_MS   2500

/** Number of features in the extracted vector */
#define FEATURE_VECTOR_DIM          6

/** Threshold for "motion active" detection in accel magnitude units.
 *  Values above this indicate the dog is not stationary.
 *  Expressed in mg (milli-g). Tuned for collar-mounted orientation.
 */
#define MOTION_THRESHOLD_MG         120

/** Zero-crossing hysteresis threshold (mg) to avoid noise triggers */
#define ZERO_CROSS_HYSTERESIS_MG    50

/* =========================================================================
 * TinyML Classifier Configuration
 * ========================================================================= */

/** Classification inference period in milliseconds.
 *  DESIGN DECISION: Runs every 5 seconds (every 2 feature windows).
 *  This provides a good balance between responsiveness and power
 *  consumption. State changes in canine behaviour are typically
 *  sustained over >5s, so this cadence is sufficient.
 */
#define CLASSIFIER_PERIOD_MS        5000

/** Number of behavioural classes */
#define NUM_BEHAVIOUR_CLASSES       5

/** Confidence threshold (%) below which classification is "uncertain" */
#define CONFIDENCE_THRESHOLD_PCT    60

/** Anxiety spike detection: if classifier outputs ANXIOUS_PACING or
 *  ALERT_FREEZE with confidence above this threshold, trigger the
 *  high-priority interrupt path.
 */
#define ANXIETY_SPIKE_CONFIDENCE    70

/* =========================================================================
 * Circular Buffer Configuration
 * ========================================================================= */

/** Circular buffer capacity in samples.
 *  DESIGN DECISION: Must be power-of-2 for fast modulo via bitmask.
 *  256 samples = ~5.12 seconds at 50 Hz. This provides enough
 *  headroom for the feature extractor to consume a 125-sample window
 *  even if it is briefly delayed by lower-priority preemption.
 */
#define CIRC_BUFFER_CAPACITY        256

/** Bitmask for fast modulo on circular buffer index */
#define CIRC_BUFFER_MASK            (CIRC_BUFFER_CAPACITY - 1)

/* =========================================================================
 * BLE Configuration
 * ========================================================================= */

/** BLE connection interval target in units of 1.25ms.
 *  24 * 1.25ms = 30ms — matches the anxiety alert latency target.
 */
#define BLE_CONN_INTERVAL_MIN       24
#define BLE_CONN_INTERVAL_MAX       40

/** Maximum BLE event queue depth (message buffer) */
#define BLE_EVENT_QUEUE_DEPTH       8

/** BLE event message size in bytes (sizeof ble_event_t) */
#define BLE_EVENT_MSG_SIZE          12

/** Offline state history buffer size (states stored when BLE disconnected).
 *  Stores up to 24 hours of state transitions assuming ~2 transitions/min.
 */
#define OFFLINE_HISTORY_CAPACITY    2880

/* =========================================================================
 * μT-Kernel 3.0 Task Configuration
 *
 * DESIGN DECISION on priority assignment:
 *   Priority 1  (highest) → IMU Sampler: must never miss a 50Hz deadline
 *   Priority 5            → Feature Extractor: time-critical but not ISR-level
 *   Priority 10           → Classifier: inference can tolerate slight jitter
 *   Priority 15 (lowest)  → BLE Logger: best-effort, no hard deadline
 *
 * Lower numeric value = higher priority in μT-Kernel 3.0.
 * ========================================================================= */

#define TASK_PRI_IMU_SAMPLER        1
#define TASK_PRI_FEATURE_EXTRACT    5
#define TASK_PRI_CLASSIFIER         10
#define TASK_PRI_BLE_LOGGER         15

/** Task stack sizes in bytes.
 *  DESIGN DECISION: Classifier gets the largest stack because the
 *  inference engine uses local arrays for intermediate activations.
 */
#define TASK_STACK_IMU_SAMPLER      512
#define TASK_STACK_FEATURE_EXTRACT  1024
#define TASK_STACK_CLASSIFIER       2048
#define TASK_STACK_BLE_LOGGER       1024

/* =========================================================================
 * Event Flag Bit Definitions
 *
 * Single event flag object with distinct bits for each inter-task signal.
 * ========================================================================= */

/** Set by IMU Sampler when a full window (125 samples) is ready */
#define EVT_NEW_SAMPLES             (0x00000001UL)

/** Set by Feature Extractor when feature vector computation is complete */
#define EVT_FEATURES_READY          (0x00000002UL)

/** Set by Classifier when an anxiety spike is detected —
 *  triggers high-priority BLE alert path */
#define EVT_ANXIETY_SPIKE           (0x00000004UL)

/** Set by BLE driver on successful connection event */
#define EVT_BLE_CONNECTED           (0x00000008UL)

/** Set by BLE driver on disconnection event */
#define EVT_BLE_DISCONNECTED        (0x00000010UL)

/** Set by system to request graceful shutdown */
#define EVT_SYSTEM_SHUTDOWN         (0x80000000UL)

/* =========================================================================
 * LED Matrix Configuration
 * ========================================================================= */

/** LED matrix refresh rate in Hz (must be >50 Hz to avoid flicker) */
#define LED_REFRESH_RATE_HZ         120

/** LED refresh cyclic handler period in milliseconds */
#define LED_REFRESH_PERIOD_MS       (1000 / LED_REFRESH_RATE_HZ)

/* =========================================================================
 * Speaker / Audio Alert Configuration
 * ========================================================================= */

/** Anxiety alert beep frequency in Hz */
#define SPEAKER_ALERT_FREQ_HZ       2000

/** Anxiety alert beep duration in milliseconds */
#define SPEAKER_ALERT_DURATION_MS   200

/** Number of beep repetitions for anxiety alert */
#define SPEAKER_ALERT_REPEAT        3

/** Silence gap between beeps in milliseconds */
#define SPEAKER_ALERT_GAP_MS        100

/* =========================================================================
 * I2C Configuration
 * ========================================================================= */

/** I2C clock frequency (400 kHz fast mode for sensor reads) */
#define I2C_FREQUENCY_HZ            400000UL

/** I2C transaction timeout in milliseconds */
#define I2C_TIMEOUT_MS              10

/* =========================================================================
 * Power Management
 * ========================================================================= */

/** Target average current consumption in mA (for battery life estimation) */
#define TARGET_AVG_CURRENT_MA       5

/** Deep sleep enable between task activations */
#define POWER_DEEP_SLEEP_ENABLE     1

#endif /* PAWSTATE_CONFIG_H */
