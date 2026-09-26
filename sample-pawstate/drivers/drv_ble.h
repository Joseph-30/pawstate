/**
 * @file drv_ble.h
 * @brief BLE GATT Abstraction Layer for PawState
 *
 * DESIGN DECISION: This module provides a clean abstraction over the
 * Nordic SoftDevice S140 BLE stack. The SoftDevice is a pre-compiled
 * binary that handles the BLE radio protocol and must be flashed
 * separately to the nRF52833. It occupies the lower portion of flash
 * (typically 0x00000000–0x00027000) and RAM (configured via linker).
 *
 * The abstraction layer allows the application tasks to call simple
 * functions like ble_notify_state() without knowing SoftDevice internals.
 * When the SoftDevice binary is linked, the _impl functions connect to
 * the actual sd_ble_* API calls.
 *
 * PawState Custom GATT Service:
 *   Service UUID: 0xAA01 (128-bit: 00001001-PAWS-TATE-0000-000000000000)
 *   Characteristics:
 *     - Current State  (UUID 0x1002): Notify, 4 bytes (state, confidence, flags)
 *     - Anxiety Alert  (UUID 0x1003): Notify, 4 bytes (high-priority path)
 *     - System Status  (UUID 0x1004): Read, 8 bytes (uptime, counts, temp)
 */

#ifndef DRV_BLE_H
#define DRV_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "pawstate_types.h"

/* BLE Service UUIDs (16-bit short forms for development).
 * 0xAA01 is in the custom/vendor-specific range for development.
 * Production should use a full 128-bit UUID. */
#define BLE_PAWSTATE_SERVICE_UUID       0xAA01
#define BLE_CHAR_CURRENT_STATE_UUID     0x1002
#define BLE_CHAR_ANXIETY_ALERT_UUID     0x1003
#define BLE_CHAR_SYSTEM_STATUS_UUID     0x1004

/** Initialise BLE stack, register GATT services, start advertising.
 *  Returns true on success. */
bool ble_init(void);

/** Check if a central device (phone) is connected */
bool ble_is_connected(void);

/** Send a state update notification to connected device.
 *  Returns true if notification was sent successfully. */
bool ble_notify_state(const ble_event_t *event);

/** Send a high-priority anxiety spike notification.
 *  DESIGN DECISION: Separate from ble_notify_state to allow
 *  the BLE logger to prioritise anxiety alerts. Internally
 *  uses the same GATT mechanism but on a different characteristic
 *  with a higher notification priority. */
bool ble_notify_anxiety_spike(const ble_event_t *event);

/** Update the readable system status characteristic */
bool ble_update_status(const system_status_t *status);

/** Start BLE advertising (called after init or disconnect) */
void ble_start_advertising(void);

/** Stop BLE advertising */
void ble_stop_advertising(void);

/** Process BLE events — call periodically from BLE logger task.
 *  Handles connection/disconnection events and CCCD writes. */
void ble_process_events(void);

/** Get the current connection handle (0xFFFF if not connected) */
uint16_t ble_get_conn_handle(void);

/** De-initialise BLE stack */
void ble_deinit(void);

#endif /* DRV_BLE_H */
