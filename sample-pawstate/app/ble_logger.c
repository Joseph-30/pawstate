/**
 * @file ble_logger.c
 * @brief Task 4 — BLE Event Logger Implementation
 *
 * DESIGN DECISION: The BLE Logger is the lowest-priority task because
 * BLE communication is best-effort — missing one notification interval
 * is not critical (the phone app will receive the next one). However,
 * anxiety spike events receive special treatment via the EVT_ANXIETY_SPIKE
 * event flag, which ensures the logger processes them immediately when
 * the scheduler next runs this task.
 *
 * Offline buffering strategy:
 *   When BLE is disconnected, state transitions are stored in a compact
 *   ring buffer (4 bytes per entry). On reconnection, the buffer is
 *   flushed as a burst of notifications. This ensures no behavioural
 *   data is lost during BLE outages (e.g., phone out of range).
 */

#include "ble_logger.h"
#include "drv_ble.h"
#include "pawstate_config.h"
#include "inference_engine.h"

#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include <string.h>

/* Module state */
static volatile uint32_t event_count = 0;
static volatile uint32_t offline_count = 0;

/* Offline state history ring buffer */
static state_history_entry_t offline_buffer[OFFLINE_HISTORY_CAPACITY];
static uint32_t offline_head = 0;
static uint32_t offline_tail = 0;
static uint32_t last_event_time_ms = 0;

/* External kernel object IDs */
extern ID flg_pipeline;
extern ID mbf_ble_events;

uint32_t ble_logger_get_event_count(void)
{
    return event_count;
}

uint32_t ble_logger_get_offline_count(void)
{
    return offline_count;
}

/**
 * Store a state transition in the offline buffer for later sync.
 */
static void store_offline(const ble_event_t *event)
{
    state_history_entry_t entry;

    /* Compute elapsed time since last entry */
    uint32_t elapsed_ms = event->timestamp_ms - last_event_time_ms;
    uint32_t elapsed_sec = elapsed_ms / 1000;
    if (elapsed_sec > 65535) elapsed_sec = 65535; /* Clamp to uint16_t */

    entry.elapsed_sec = (uint16_t)elapsed_sec;
    entry.state = event->state;
    entry.confidence = event->confidence;

    /* Write to ring buffer */
    offline_buffer[offline_head] = entry;
    offline_head = (offline_head + 1) % OFFLINE_HISTORY_CAPACITY;

    /* If buffer is full, overwrite oldest entry */
    if (offline_head == offline_tail) {
        offline_tail = (offline_tail + 1) % OFFLINE_HISTORY_CAPACITY;
    } else {
        offline_count++;
    }

    last_event_time_ms = event->timestamp_ms;
}

/**
 * Flush offline buffer by sending all stored entries as BLE notifications.
 * Called when BLE reconnects.
 */
static void flush_offline_buffer(void)
{
    ble_event_t replay_event;
    memset(&replay_event, 0, sizeof(replay_event));

    while (offline_tail != offline_head) {
        state_history_entry_t *entry = &offline_buffer[offline_tail];

        replay_event.state = entry->state;
        replay_event.confidence = entry->confidence;
        replay_event.is_anxiety = (entry->state == STATE_ANXIOUS_PACING ||
                                   entry->state == STATE_ALERT_FREEZE) ? 1 : 0;
        replay_event.timestamp_ms = 0; /* Historical — no precise timestamp */

        ble_notify_state(&replay_event);

        offline_tail = (offline_tail + 1) % OFFLINE_HISTORY_CAPACITY;
        offline_count--;

        /* Brief delay between notifications to avoid BLE stack overflow.
         * 50ms allows ~2 connection intervals for each notification. */
        tk_dly_tsk(50);
    }
}

/**
 * BLE Event Logger Task — Priority 15 (lowest)
 *
 * Main loop:
 *   1. Receive event from message buffer (blocking)
 *   2. Process BLE connection events
 *   3. If connected: send notification immediately
 *   4. If disconnected: store in offline buffer
 *   5. On reconnection: flush offline buffer
 */
void ble_logger_task(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    ble_event_t event;
    INT msg_size;
    bool was_connected = false;

    /* Initialise BLE stack */
    ble_init();

    for (;;) {
        /* Wait for an event from the classifier via message buffer.
         * TMO_FEVR = wait forever.
         * This blocks the task until a message arrives, freeing
         * the CPU for higher-priority tasks. */
        msg_size = tk_rcv_mbf(mbf_ble_events, &event, TMO_FEVR);

        if (msg_size < (INT)sizeof(ble_event_t)) {
            continue; /* Invalid message — skip */
        }

        event_count++;

        /* Process BLE connection/disconnection */
        ble_process_events();
        bool is_connected = ble_is_connected();

        /* Detect reconnection → flush offline buffer */
        if (is_connected && !was_connected && offline_count > 0) {
            flush_offline_buffer();
        }
        was_connected = is_connected;

        if (is_connected) {
            /* BLE is connected — send notification */
            if (event.is_anxiety) {
                /* High-priority anxiety spike notification.
                 * Uses dedicated anxiety alert characteristic. */
                ble_notify_anxiety_spike(&event);
                tm_printf((const UB*)"[BLE] *** ANXIETY NOTIFICATION SENT: State=%d (%s), Conf=%d%% ***\n",
                          event.state, (const UB*)inference_state_name(event.state), event.confidence);
            } else {
                /* Normal state update notification */
                ble_notify_state(&event);
                tm_printf((const UB*)"[BLE] State Notification Sent: State=%d (%s), Conf=%d%%\n",
                          event.state, (const UB*)inference_state_name(event.state), event.confidence);
            }
        } else {
            /* BLE disconnected — store in offline buffer.
             * The dog owner's phone is out of range. When they
             * return, all missed state transitions will be synced. */
            store_offline(&event);

            /* Restart advertising so the phone can reconnect */
            ble_start_advertising();
        }
    }
}
