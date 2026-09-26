/**
 * @file circular_buffer.h
 * @brief Lock-Free SPSC Circular Buffer for IMU Samples
 *
 * Single-Producer Single-Consumer ring buffer using volatile atomic
 * head/tail indices. The producer (IMU Sampler task) writes samples,
 * the consumer (Feature Extractor task) reads windows.
 *
 * DESIGN DECISION: Lock-free because the producer runs at Priority 1
 * and must never block on a mutex. Power-of-2 capacity enables fast
 * modulo via bitmask. No dynamic allocation — statically allocated.
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "pawstate_types.h"
#include "pawstate_config.h"

typedef struct {
    imu_sample_t samples[CIRC_BUFFER_CAPACITY];
    volatile uint32_t head;  /* Write index (producer only) */
    volatile uint32_t tail;  /* Read index  (consumer only) */
} circular_buffer_t;

/** Initialise the circular buffer (zero head/tail) */
void cbuf_init(circular_buffer_t *cb);

/** Write one sample. Returns true on success, false if buffer is full.
 *  Called from producer (IMU Sampler) only — no locking needed. */
bool cbuf_write(circular_buffer_t *cb, const imu_sample_t *sample);

/** Read one sample. Returns true on success, false if buffer is empty.
 *  Called from consumer (Feature Extractor) only. */
bool cbuf_read(circular_buffer_t *cb, imu_sample_t *sample);

/** Read a contiguous window of `count` samples WITHOUT consuming them.
 *  Copies into `out` array. Returns actual number of samples copied.
 *  Used by Feature Extractor to read a 125-sample window. */
uint32_t cbuf_peek_window(const circular_buffer_t *cb, imu_sample_t *out,
                          uint32_t count);

/** Discard `count` samples from the read side (advance tail). */
void cbuf_consume(circular_buffer_t *cb, uint32_t count);

/** Number of samples currently available for reading */
uint32_t cbuf_available(const circular_buffer_t *cb);

/** Check if buffer is empty */
bool cbuf_is_empty(const circular_buffer_t *cb);

/** Check if buffer is full */
bool cbuf_is_full(const circular_buffer_t *cb);

#endif /* CIRCULAR_BUFFER_H */
