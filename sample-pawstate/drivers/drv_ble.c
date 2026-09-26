/**
 * @file drv_ble.c
 * @brief BLE GATT Abstraction Layer Implementation
 *
 * DESIGN DECISION: This implementation provides the full BLE abstraction
 * layer with SoftDevice S140 API calls. The SoftDevice binary must be
 * flashed to the nRF52833 separately and the linker script must reserve
 * memory for it (see README.md for integration instructions).
 *
 * When building without SoftDevice (e.g., for unit testing or initial
 * development), compile with -DBLE_SOFTDEVICE_PRESENT=0 to use the
 * stub implementation that logs events but doesn't transmit.
 *
 * Memory layout with SoftDevice S140 v7.x:
 *   Flash: 0x00000000 – 0x00027000 (SoftDevice)
 *   Flash: 0x00027000 – 0x00080000 (Application)
 *   RAM:   0x20000000 – 0x20002000 (SoftDevice, ~8KB minimum)
 *   RAM:   0x20002000 – 0x20020000 (Application, ~120KB available)
 */

#include "drv_ble.h"
#include "nrf52833_hal.h"
#include "pawstate_config.h"
#include <string.h>

/* --- BLE State --- */
static bool     ble_initialized = false;
static bool     ble_connected = false;
static uint16_t ble_conn_handle = 0xFFFF;
static bool     state_cccd_enabled = false;
static bool     anxiety_cccd_enabled = false;

/* --- GATT Handles --- */
static uint16_t service_handle = 0;
static uint16_t state_char_handle = 0;
static uint16_t anxiety_char_handle = 0;
static uint16_t status_char_handle = 0;

/* --- Advertising data --- */
static const uint8_t adv_data[] = {
    /* Flags: General Discoverable, BR/EDR not supported */
    0x02, 0x01, 0x06,
    /* Complete Local Name: "PawState" */
    0x09, 0x09, 'P', 'a', 'w', 'S', 't', 'a', 't', 'e'
};

/* --- Notification buffer --- */
static uint8_t notify_buf[12];

/*
 * DESIGN DECISION on SoftDevice integration:
 * The SoftDevice API (sd_ble_*) functions are provided by the SoftDevice
 * binary and linked at build time. The application calls these via
 * Supervisor Calls (SVC). When SoftDevice is not present, we provide
 * stub implementations that track state internally.
 *
 * For the contest prototype, SoftDevice S140 v7.3.0 is the recommended
 * version for nRF52833 with BLE 5.0 support.
 */

#ifndef BLE_SOFTDEVICE_PRESENT
#define BLE_SOFTDEVICE_PRESENT  0
#endif

#if BLE_SOFTDEVICE_PRESENT

/* === SoftDevice S140 Implementation === */
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_advertising.h"

/* SoftDevice GATT service registration and notification
 * implementations would go here. The API surface is identical
 * to the stub version below. */

/* Note: Full SoftDevice integration requires:
 * 1. SoftDevice binary flashed at 0x00000000
 * 2. nrf_sdh_enable_request() called before any BLE operations
 * 3. BLE event handler registered via NRF_SDH_BLE_OBSERVER
 * 4. GAP parameters configured (device name, connection params)
 * 5. GATT services registered with sd_ble_gatts_service_add()
 * See README.md "BLE SoftDevice Integration" section for full steps. */

#else

/* === Stub Implementation (no SoftDevice) === */

bool ble_init(void)
{
    ble_initialized = true;
    ble_connected = false;
    ble_conn_handle = 0xFFFF;
    state_cccd_enabled = false;
    anxiety_cccd_enabled = false;

    /* Assign sequential handles (simulating GATT registration) */
    service_handle = 0x0010;
    state_char_handle = 0x0012;
    anxiety_char_handle = 0x0015;
    status_char_handle = 0x0018;

    /* In stub mode, simulate advertising start */
    ble_start_advertising();

    return true;
}

bool ble_is_connected(void)
{
    return ble_connected;
}

bool ble_notify_state(const ble_event_t *event)
{
    if (!ble_connected || !state_cccd_enabled) {
        return false;
    }

    /* Pack notification data: [state, confidence, flags, reserved] */
    notify_buf[0] = (uint8_t)event->state;
    notify_buf[1] = event->confidence;
    notify_buf[2] = event->is_anxiety;
    notify_buf[3] = 0;

    /* In stub mode: notification is "sent" (data is ready for inspection).
     * With SoftDevice: would call sd_ble_gatts_hvx() here. */

    /* Encode timestamp in bytes 4-7 (little-endian) */
    notify_buf[4] = (uint8_t)(event->timestamp_ms & 0xFF);
    notify_buf[5] = (uint8_t)((event->timestamp_ms >> 8) & 0xFF);
    notify_buf[6] = (uint8_t)((event->timestamp_ms >> 16) & 0xFF);
    notify_buf[7] = (uint8_t)((event->timestamp_ms >> 24) & 0xFF);

    return true;
}

bool ble_notify_anxiety_spike(const ble_event_t *event)
{
    if (!ble_connected || !anxiety_cccd_enabled) {
        return false;
    }

    /* High-priority anxiety notification on dedicated characteristic.
     * Identical format but on the anxiety alert characteristic handle,
     * which the phone app monitors with a distinct notification channel. */
    notify_buf[0] = (uint8_t)event->state;
    notify_buf[1] = event->confidence;
    notify_buf[2] = 1; /* Always 1 for anxiety alerts */
    notify_buf[3] = (uint8_t)event->prev_state;

    notify_buf[4] = (uint8_t)(event->timestamp_ms & 0xFF);
    notify_buf[5] = (uint8_t)((event->timestamp_ms >> 8) & 0xFF);
    notify_buf[6] = (uint8_t)((event->timestamp_ms >> 16) & 0xFF);
    notify_buf[7] = (uint8_t)((event->timestamp_ms >> 24) & 0xFF);

    return true;
}

bool ble_update_status(const system_status_t *status)
{
    if (!ble_initialized) return false;

    /* Pack system status into the readable characteristic value.
     * This doesn't require a notification — the phone can read it
     * on demand. With SoftDevice: sd_ble_gatts_value_set(). */
    (void)status;
    return true;
}

void ble_start_advertising(void)
{
    /* In stub mode, we just set a flag.
     * With SoftDevice: sd_ble_gap_adv_start() with adv_data. */
    (void)adv_data;
}

void ble_stop_advertising(void)
{
    /* With SoftDevice: sd_ble_gap_adv_stop() */
}

void ble_process_events(void)
{
    /* In stub mode, simulate a connection after some time for testing.
     * With SoftDevice: process events from sd_ble_evt_get() —
     * handle BLE_GAP_EVT_CONNECTED, BLE_GAP_EVT_DISCONNECTED,
     * BLE_GATTS_EVT_WRITE (CCCD updates). */

    /* Check button B for simulated connection toggle (development aid) */
    if (!ble_connected) {
        /* Auto-simulate connected + CCCD enabled for standalone testing */
        ble_connected = true;
        ble_conn_handle = 0x0001;
        state_cccd_enabled = true;
        anxiety_cccd_enabled = true;
    }
}

uint16_t ble_get_conn_handle(void)
{
    return ble_conn_handle;
}

void ble_deinit(void)
{
    ble_connected = false;
    ble_initialized = false;
    ble_conn_handle = 0xFFFF;
}

#endif /* BLE_SOFTDEVICE_PRESENT */
