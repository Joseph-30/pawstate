/**
 * @file inference_engine.c
 * @brief INT8 FC Neural Network Inference — Pure C Implementation
 *
 * DESIGN DECISION: Custom inference engine instead of TF-Lite Micro.
 * Rationale:
 *   1. TF-Lite Micro requires C++ and adds ~50-100KB flash overhead
 *   2. Our model is a simple 3-layer FC network (no conv, no LSTM)
 *   3. Custom C implementation is ~500 bytes of code vs ~80KB for TFLM
 *   4. Deterministic execution time (no interpreter overhead)
 *   5. μT-Kernel 3.0 is a pure C RTOS — C++ adds toolchain complexity
 *
 * The inference pipeline:
 *   1. Quantise Q16.16 features to INT8 input
 *   2. FC1: INT8 weights × INT8 input → INT32 accumulator + bias → ReLU → INT8
 *   3. FC2: same process
 *   4. FC3: same, but output goes through softmax instead of ReLU
 *   5. Argmax of softmax output → predicted class + confidence
 */

#include "inference_engine.h"
#include "model_data.h"
#include "pawstate_config.h"

/* --- State name strings for debug --- */
static const char *state_names[] = {
    "Resting",
    "Walking",
    "Playing",
    "Anxious Pacing",
    "Alert Freeze"
};



/**
 * ReLU activation on INT32 accumulator, then requantise to INT8.
 * DESIGN DECISION: Right-shift by 8 as a simple requantisation from
 * INT32 accumulator range back to INT8 range. This is equivalent to
 * a fixed output scale of 1/256.
 */
static int8_t relu_requantise(int32_t acc)
{
    /* ReLU: clamp negative to zero */
    if (acc < 0) acc = 0;

    /* Requantise INT32 → INT8 by right-shifting */
    int32_t result = acc >> 8;

    if (result > 127) return 127;
    return (int8_t)result;
}

/**
 * Softmax over INT32 array, outputting uint8_t probabilities (0-255).
 *
 * DESIGN DECISION: Using a simplified softmax that avoids exp().
 * Instead, we subtract the max value (for numerical stability),
 * use a piecewise-linear approximation of exp, then normalise.
 * This is standard practice in quantised inference on Cortex-M.
 */
static void softmax_int32(const int32_t *input, uint8_t *output, int count)
{
    /* Find maximum for numerical stability */
    int32_t max_val = input[0];
    for (int i = 1; i < count; i++) {
        if (input[i] > max_val) max_val = input[i];
    }

    /* Compute approximate exp(x - max) using shift-based approximation.
     * For each value, compute a positive "score" proportional to exp(x).
     * We use: score = max(1, 256 + (x - max) * 4)
     * This gives higher scores to larger values with a linear ramp. */
    uint32_t scores[MODEL_OUTPUT_DIM];
    uint32_t sum = 0;

    for (int i = 0; i < count; i++) {
        int32_t shifted = (input[i] - max_val);
        /* Scale by 4 to spread out differences */
        int32_t score = 256 + (shifted * 4);
        if (score < 1) score = 1;
        scores[i] = (uint32_t)score;
        sum += scores[i];
    }

    /* Normalise to 0-255 range */
    if (sum == 0) sum = 1; /* Prevent division by zero */

    for (int i = 0; i < count; i++) {
        output[i] = (uint8_t)((scores[i] * 255) / sum);
    }
}

bool inference_init(void)
{
    /* Validate model dimensions match configuration */
    if (MODEL_INPUT_DIM != FEATURE_VECTOR_DIM) return false;
    if (MODEL_OUTPUT_DIM != NUM_BEHAVIOUR_CLASSES) return false;

    /* Model data is const in flash — no initialisation needed */
    return true;
}

bool inference_run(const feature_vector_t *features,
                   classifier_result_t *result)
{
    /* === Step 1: Quantise input features to INT8 === */
    int8_t input[MODEL_INPUT_DIM];
    const q16_16_t *feat_array = (const q16_16_t *)features;

    for (int i = 0; i < MODEL_INPUT_DIM; i++) {
        /* Convert Q16.16 to float, multiply by optimal scaling factor, then cast to INT8 */
        float val = (float)feat_array[i] / 65536.0f;
        float scaled = val * feature_quantization_factors[i];
        
        if (scaled > 127.0f) scaled = 127.0f;
        if (scaled < -128.0f) scaled = -128.0f;
        
        input[i] = (int8_t)scaled;
    }

    /* === Step 2: FC Layer 1 — input(6) → hidden1(16) + ReLU === */
    int8_t hidden1[MODEL_HIDDEN1_DIM];

    for (int o = 0; o < MODEL_HIDDEN1_DIM; o++) {
        int32_t acc = fc1_biases[o];

        for (int i = 0; i < MODEL_INPUT_DIM; i++) {
            /* INT8 × INT8 → INT16, accumulated in INT32 */
            acc += (int32_t)fc1_weights[o][i] * (int32_t)input[i];
        }

        hidden1[o] = relu_requantise(acc);
    }

    /* === Step 3: FC Layer 2 — hidden1(16) → hidden2(8) + ReLU === */
    int8_t hidden2[MODEL_HIDDEN2_DIM];

    for (int o = 0; o < MODEL_HIDDEN2_DIM; o++) {
        int32_t acc = fc2_biases[o];

        for (int i = 0; i < MODEL_HIDDEN1_DIM; i++) {
            acc += (int32_t)fc2_weights[o][i] * (int32_t)hidden1[i];
        }

        hidden2[o] = relu_requantise(acc);
    }

    /* === Step 4: FC Layer 3 (Output) — hidden2(8) → output(5) === */
    int32_t logits[MODEL_OUTPUT_DIM];

    for (int o = 0; o < MODEL_OUTPUT_DIM; o++) {
        int32_t acc = fc3_biases[o];

        for (int i = 0; i < MODEL_HIDDEN2_DIM; i++) {
            acc += (int32_t)fc3_weights[o][i] * (int32_t)hidden2[i];
        }

        logits[o] = acc;
    }

    /* === Step 5: Softmax → probabilities === */
    softmax_int32(logits, result->class_probs, MODEL_OUTPUT_DIM);

    /* === Step 6: Argmax → predicted class + confidence === */
    uint8_t max_prob = 0;
    uint8_t max_class = 0;

    for (int i = 0; i < MODEL_OUTPUT_DIM; i++) {
        if (result->class_probs[i] > max_prob) {
            max_prob = result->class_probs[i];
            max_class = (uint8_t)i;
        }
    }

    result->predicted_class = (behaviour_state_t)max_class;

    /* Convert 0-255 probability to 0-100% confidence */
    result->confidence = (uint8_t)((uint32_t)max_prob * 100 / 255);

    /* Anxiety spike detection:
     * Triggered when classifier predicts ANXIOUS_PACING or ALERT_FREEZE
     * with sufficient confidence. */
    result->is_anxiety_spike = false;
    if ((result->predicted_class == STATE_ANXIOUS_PACING ||
         result->predicted_class == STATE_ALERT_FREEZE) &&
        result->confidence >= ANXIETY_SPIKE_CONFIDENCE) {
        result->is_anxiety_spike = true;
    }

    return true;
}

const char *inference_state_name(behaviour_state_t state)
{
    if (state < NUM_BEHAVIOUR_CLASSES) {
        return state_names[state];
    }
    return "Unknown";
}
