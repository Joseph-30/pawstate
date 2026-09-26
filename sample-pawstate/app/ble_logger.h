/**
 * @file ble_logger.h
 * @brief Task 4 — BLE Event Logger (Priority 15, Lowest)
 *
 * Receives state-change events from the classifier via message buffer
 * and sends BLE GATT notifications to the smartphone app. Handles
 * both normal state updates and high-priority anxiety spike alerts.
 * Buffers events offline when BLE is disconnected.
 */

#ifndef BLE_LOGGER_H
#define BLE_LOGGER_H

#include <tk/tkernel.h>
#include "pawstate_types.h"

/** BLE Event Logger task entry point */
void ble_logger_task(INT stacd, void *exinf);

/** Get the number of events logged since boot */
uint32_t ble_logger_get_event_count(void);

/** Get the number of offline-buffered events awaiting sync */
uint32_t ble_logger_get_offline_count(void);

#endif /* BLE_LOGGER_H */
