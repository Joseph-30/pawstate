/**
 * @file tinyml_classifier.c
 * @brief Task 3 — TinyML Classifier Implementation
 *
 * DESIGN DECISION: The classifier runs every time the Feature Extractor
 * signals EVT_FEATURES_READY. This decouples the classification rate
 * from a fixed timer — if feature extraction is delayed, the classifier
 * simply waits. This provides better temporal accuracy than a fixed 5s
 * timer because the classification always uses the freshest features.
 *
 * Anxiety spike detection creates a HIGH-PRIORITY event path:
 *   Classifier detects anxiety → Sets EVT_ANXIETY_SPIKE flag
 *     → BLE Logger is woken at elevated effective priority
 *     → Immediate BLE notification within ~30ms
 *     → Speaker beep alert on-board
 *     → LED shows anxiety pattern
 *
 * This interrupt-like path is the key RTOS advantage over bare-metal:
 * a bare-metal loop would have to wait for the BLE transmit slot,
 * but μT-Kernel's preemptive scheduling ensures the alert task runs
 * immediately after the classifier sets the flag.
 */

#include "tinyml_classifier.h"
#include "feature_extractor.h"
#include "inference_engine.h"
#include "drv_led_matrix.h"
#include "drv_gpio.h"
#include "pawstate_config.h"

#include <tk/tkernel.h>
#include <tm/tmonitor.h>

/* Module state */
static behaviour_state_t current_state = STATE_UNKNOWN;
static behaviour_state_t previous_state = STATE_UNKNOWN;
static uint8_t current_confidence = 0;
static volatile uint32_t inference_count = 0;

/* External kernel object IDs */
extern ID flg_pipeline;
extern ID sem_feature_buf;
extern ID mbf_ble_events;

behaviour_state_t classifier_get_state(void)
{
    return current_state;
}

uint8_t classifier_get_confidence(void)
{
    return current_confidence;
}

uint32_t classifier_get_inference_count(void)
{
    return inference_count;
}

/**
 * Build a BLE event structure and send to the message buffer.
 * Called for both normal state changes and anxiety spikes.
 */
static void send_ble_event(behaviour_state_t state, uint8_t confidence,
                           bool is_anxiety, behaviour_state_t prev)
{
    ble_event_t event;
    event.state = state;
    event.confidence = confidence;
    event.is_anxiety = is_anxiety ? 1 : 0;
    event.reserved = 0;
    event.prev_state = prev;
    event.padding[0] = 0;
    event.padding[1] = 0;
    event.padding[2] = 0;

    /* Get system time for timestamp.
     * DESIGN DECISION: Using tk_get_tim for millisecond timestamp.
     * On 32-bit, this wraps after ~49 days — acceptable for a collar
     * that is recharged more frequently. */
    SYSTIM tim;
    tk_get_tim(&tim);
    event.timestamp_ms = (uint32_t)(tim.lo);

    /* Send to message buffer (non-blocking).
     * TMO_POL = polling mode — if buffer is full, drop the event.
     * This prevents the classifier from blocking on a full BLE queue. */
    tk_snd_mbf(mbf_ble_events, &event, sizeof(ble_event_t), TMO_POL);
}

/**
 * TinyML Classifier Task — Priority 10
 *
 * Waits for features, runs inference, detects state transitions,
 * triggers anxiety alerts, and updates LED display.
 */
void tinyml_classifier_task(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    UINT flg_pattern;
    classifier_result_t result;
    feature_vector_t local_features;

    /* Initialise inference engine */
    inference_init();

    for (;;) {
        /* Wait for feature vector to be ready */
        tk_wai_flg(flg_pipeline, EVT_FEATURES_READY,
                   TWF_ORW | TWF_CLR, &flg_pattern, TMO_FEVR);

        /* Copy feature vector under semaphore protection.
         * DESIGN DECISION: We copy to a local variable and release
         * the semaphore immediately, so the Feature Extractor can
         * begin computing the next window while we run inference. */
        tk_wai_sem(sem_feature_buf, 1, TMO_FEVR);
        local_features = *feature_extractor_get_features();
        tk_sig_sem(sem_feature_buf, 1);

        /* Run neural network inference */
        tm_printf("INF#%lu\n", (unsigned long)inference_count);
        if (!inference_run(&local_features, &result)) {
            /* Inference failed — keep previous state */
            continue;
        }

        inference_count++;

        /* Apply confidence threshold with temporal debouncing (persistence filter) */
        static uint8_t uncertain_streak = 0;
        static behaviour_state_t pending_state = STATE_UNKNOWN;
        static uint8_t pending_count = 0;

        behaviour_state_t new_state;

        if (result.confidence >= CONFIDENCE_THRESHOLD_PCT) {
            uncertain_streak = 0;
            
            /* High-priority anxiety spikes (pacing or freeze >= ANXIETY_SPIKE_CONFIDENCE)
             * take effect IMMEDIATELY without debounce */
            if (result.is_anxiety_spike) {
                new_state = result.predicted_class;
                pending_count = 0;
            }
            /* High confidence (>= 70%) switches state immediately */
            else if (result.confidence >= 70) {
                new_state = result.predicted_class;
                pending_count = 0;
            }
            /* Moderate confidence (50-69%): require 2 consecutive windows or same state */
            else if (result.predicted_class == current_state) {
                new_state = current_state;
                pending_count = 0;
            } else if (result.predicted_class == pending_state) {
                pending_count++;
                if (pending_count >= 2) {
                    new_state = pending_state;
                    pending_count = 0;
                } else {
                    new_state = (current_state != STATE_UNKNOWN) ? current_state : result.predicted_class;
                }
            } else {
                pending_state = result.predicted_class;
                pending_count = 1;
                new_state = (current_state != STATE_UNKNOWN) ? current_state : result.predicted_class;
            }
        } else {
            /* Low confidence (< 50%):
             * If currently in a valid state, hold it for 1 window before falling back to UNKNOWN.
             * This prevents a single ambiguous window from triggering the '?' icon loop. */
            uncertain_streak++;
            if (uncertain_streak >= 2 || current_state == STATE_UNKNOWN) {
                new_state = STATE_UNKNOWN;
            } else {
                new_state = current_state; /* Hold previous known state */
            }
        }

        /* Detect state transitions */
        bool state_changed = (new_state != current_state);
        previous_state = current_state;
        current_state = new_state;
        current_confidence = result.confidence;

        /* DEBUG: Print probabilities in minimal format */
        tm_printf("P:%d,%d,%d,%d,%d\n",
                 result.class_probs[0], result.class_probs[1], result.class_probs[2],
                 result.class_probs[3], result.class_probs[4]);

        if (state_changed) {
            tm_printf((const UB*)"[ML] State changed to: %s (Confidence: %d%%)\n", 
                      (const UB*)inference_state_name(current_state), current_confidence);
        }

        /* Update LED display to show current state */
        led_matrix_show_state(current_state);

        /* Check for anxiety spike (high-priority path) */
        if (result.is_anxiety_spike && state_changed) {
            /* === ANXIETY SPIKE DETECTED ===
             *
             * This is the critical interrupt-like path that justifies
             * using an RTOS. The sequence is:
             *   1. Set EVT_ANXIETY_SPIKE flag → wakes BLE Logger
             *   2. Send event to message buffer with is_anxiety=1
             *   3. Play speaker alert (beep pattern)
             *
             * Step 1 causes μT-Kernel to immediately check if the
             * BLE Logger should preempt. Since we're at Priority 10
             * and BLE Logger is at Priority 15, it won't preempt us
             * directly — but the flag ensures the BLE Logger runs
             * as soon as we go back to sleep (or a higher-priority
             * task finishes).
             *
             * DESIGN DECISION: We play the speaker alert here in the
             * classifier task rather than in the BLE Logger because:
             *   - The speaker should fire regardless of BLE connection
             *   - The BLE Logger might be delayed if BLE is busy
             *   - Audio feedback is immediate; BLE notification follows
             */

            /* Signal BLE Logger for immediate notification */
            tk_set_flg(flg_pipeline, EVT_ANXIETY_SPIKE);

            /* Send high-priority event to message buffer */
            send_ble_event(current_state, current_confidence,
                          true, previous_state);

            tm_printf((const UB*)"[ML] *** ANXIETY SPIKE DETECTED! Triggering Alarm ***\n");

            /* Audible alert via on-board speaker */
            gpio_speaker_anxiety_alert();

        } else if (state_changed && current_state != STATE_UNKNOWN) {
            /* Normal state transition — send regular BLE event */
            send_ble_event(current_state, current_confidence,
                          false, previous_state);
        }
    }
}
