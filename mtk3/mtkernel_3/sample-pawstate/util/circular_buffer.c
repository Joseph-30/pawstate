/**
 * @file circular_buffer.c
 * @brief Lock-Free SPSC Circular Buffer Implementation
 *
 * DESIGN DECISION: No memory barriers are explicitly needed on ARM Cortex-M4
 * for SPSC with volatile indices, because Cortex-M4 has a strongly-ordered
 * memory model for normal memory. The volatile qualifier prevents compiler
 * reordering, and hardware ensures write visibility.
 */

#include "circular_buffer.h"
#include <string.h>

void cbuf_init(circular_buffer_t *cb)
{
    cb->head = 0;
    cb->tail = 0;
    /* Zero the sample buffer for deterministic startup */
    memset(cb->samples, 0, sizeof(cb->samples));
}

bool cbuf_write(circular_buffer_t *cb, const imu_sample_t *sample)
{
    uint32_t next_head = (cb->head + 1) & CIRC_BUFFER_MASK;

    if (next_head == cb->tail) {
        /* Buffer is full — drop the sample.
         * DESIGN DECISION: Dropping is acceptable because the IMU Sampler
         * must never block. In practice, the Feature Extractor consumes
         * fast enough that this should not occur. */
        return false;
    }

    cb->samples[cb->head] = *sample;
    cb->head = next_head;
    return true;
}

bool cbuf_read(circular_buffer_t *cb, imu_sample_t *sample)
{
    if (cb->tail == cb->head) {
        return false; /* Empty */
    }

    *sample = cb->samples[cb->tail];
    cb->tail = (cb->tail + 1) & CIRC_BUFFER_MASK;
    return true;
}

uint32_t cbuf_peek_window(const circular_buffer_t *cb, imu_sample_t *out,
                          uint32_t count)
{
    uint32_t available = cbuf_available(cb);
    if (count > available) {
        count = available;
    }

    uint32_t idx = cb->tail;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = cb->samples[idx];
        idx = (idx + 1) & CIRC_BUFFER_MASK;
    }

    return count;
}

void cbuf_consume(circular_buffer_t *cb, uint32_t count)
{
    uint32_t available = cbuf_available(cb);
    if (count > available) {
        count = available;
    }
    cb->tail = (cb->tail + count) & CIRC_BUFFER_MASK;
}

uint32_t cbuf_available(const circular_buffer_t *cb)
{
    return (cb->head - cb->tail) & CIRC_BUFFER_MASK;
}

bool cbuf_is_empty(const circular_buffer_t *cb)
{
    return (cb->head == cb->tail);
}

bool cbuf_is_full(const circular_buffer_t *cb)
{
    return (((cb->head + 1) & CIRC_BUFFER_MASK) == cb->tail);
}
