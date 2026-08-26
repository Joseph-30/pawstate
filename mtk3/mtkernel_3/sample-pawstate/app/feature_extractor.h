/**
 * @file feature_extractor.h
 * @brief Task 2 — Feature Extractor (Priority 5)
 *
 * Computes a 6-dimensional feature vector from a 125-sample sliding
 * window of IMU data. Signals the classifier when features are ready.
 */

#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <tk/tkernel.h>
#include "pawstate_types.h"

/** Feature Extractor task entry point */
void feature_extractor_task(INT stacd, void *exinf);

/** Get pointer to the latest computed feature vector.
 *  Caller must hold sem_feature_buf before reading. */
const feature_vector_t *feature_extractor_get_features(void);

#endif /* FEATURE_EXTRACTOR_H */
