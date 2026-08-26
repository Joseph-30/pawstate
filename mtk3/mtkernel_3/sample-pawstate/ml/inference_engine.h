/**
 * @file inference_engine.h
 * @brief Lightweight INT8 Fully-Connected Neural Network Inference Engine
 *
 * Pure C implementation — no C++, no TF-Lite dependency, no dynamic allocation.
 * Processes: FC(ReLU) → FC(ReLU) → FC(Softmax) with INT8 weights and
 * INT32 accumulation for numerical stability.
 */

#ifndef INFERENCE_ENGINE_H
#define INFERENCE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "pawstate_types.h"

/** Initialise the inference engine (validates model data) */
bool inference_init(void);

/** Run inference on a feature vector.
 *  @param features   Input feature vector (6-D, Q16.16 fixed-point)
 *  @param result     Output classification result
 *  @return true on successful inference */
bool inference_run(const feature_vector_t *features,
                   classifier_result_t *result);

/** Get the string name for a behaviour state (for debug logging) */
const char *inference_state_name(behaviour_state_t state);

#endif /* INFERENCE_ENGINE_H */
