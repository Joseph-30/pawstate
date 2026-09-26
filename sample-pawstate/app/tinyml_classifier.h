/**
 * @file tinyml_classifier.h
 * @brief Task 3 — TinyML Classifier (Priority 10)
 *
 * Runs INT8 FC neural network inference on the extracted feature vector
 * to classify the dog's behavioural state. Detects anxiety spikes and
 * triggers the high-priority BLE alert path.
 */

#ifndef TINYML_CLASSIFIER_H
#define TINYML_CLASSIFIER_H

#include <tk/tkernel.h>
#include "pawstate_types.h"

/** TinyML Classifier task entry point */
void tinyml_classifier_task(INT stacd, void *exinf);

/** Get the current behavioural state classification */
behaviour_state_t classifier_get_state(void);

/** Get the current classification confidence (0-100%) */
uint8_t classifier_get_confidence(void);

/** Get the total number of inferences performed */
uint32_t classifier_get_inference_count(void);

#endif /* TINYML_CLASSIFIER_H */
