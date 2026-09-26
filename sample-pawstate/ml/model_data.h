#ifndef MODEL_DATA_H
#define MODEL_DATA_H

#include <stdint.h>

#define MODEL_INPUT_SCALE       0.015625f
#define MODEL_INPUT_ZERO_POINT  0
#define MODEL_OUTPUT_SCALE      0.00390625f
#define MODEL_OUTPUT_ZERO_POINT (-128)

#define MODEL_INPUT_DIM         6
#define MODEL_HIDDEN1_DIM       16
#define MODEL_HIDDEN2_DIM       8
#define MODEL_OUTPUT_DIM        5

/* Per-feature quantization factors (multiply by this, then clamp to INT8) */
static const float feature_quantization_factors[MODEL_INPUT_DIM] = {
    0.002648f, /* Accel Std Dev */
    0.003642f, /* Accel Mean Mag */
    0.886608f, /* Mag RMS Delta */
    0.798417f, /* Tilt Angle Delta */
    3.846154f, /* Zero Crossings */
    0.043333f  /* Bout Duration */
};

static const int8_t fc1_weights[MODEL_HIDDEN1_DIM][MODEL_INPUT_DIM] = {
    {    8,   27,    8,  -32,   61,   -2 },
    {  -68,   66,  -59,  -31,   12,  -79 },
    {   24,  -19,   -2,   -3,   -2,  -51 },
    {   -4,   46,   49,   49,  -75,   14 },
    {   99,   11,   69,  -30,   62,  -76 },
    {   68,  -82,  -61,   -2,  -23,  -84 },
    {  -98,   52,   30,   35,  -17,  -93 },
    {   60,  -33,  -46,  -25,   59,  -11 },
    {   93,   45,  -28,   64,  -12,  -35 },
    {   44,  -30,  -63,  -33,   75,   77 },
    {   62,  -57,  -51,  -66,  -35,    1 },
    {   35,   18,  127,  -15,   20,   40 },
    {  -51,   77,    3,   -2,   36,   65 },
    {    4,  -14,   36,   55,   23, -103 },
    {   32,   49,   34,   33,  -64,    9 },
    {   55,   -9,  -73,    0,   56,   37 }
};

static const int32_t fc1_biases[MODEL_HIDDEN1_DIM] = {
        83,     22,      0,     73,    -57,      0,     28,    -59,    -39,    110,      0,    -31,    -10,     -9,    -55,   -117
};

static const int8_t fc2_weights[MODEL_HIDDEN2_DIM][MODEL_HIDDEN1_DIM] = {
    { -113,    3,   16,  -33,  -72,   10,  -25,   17,  -58,   20,   45,  -81,   86,  -96,   81,  -85 },
    {  -63,   28,  -15,  -25,  -10,   44,  -20,   56,  -97,  -26,   68,   81,  -75,  -90,  -97,  -26 },
    {    8,   84,   84,  -40,   43,   44,  127,  -15,    5,  -14,   34,  -10,   56,    3,  -41,   60 },
    {   65,  -22,  -62,   57,  -79,   48,    9,   61,   59,    7,  -30,  -52,  -70,  -57,   15,  -32 },
    {  -53,  -41,  -52,  -35,   42,    6,  -19,   76,   16,  -14,  -24,   56,  -77,   57,  -75,  -46 },
    {   57,   -8,  -64,   52,  -72,   86,  -36,   80,  -52,   41,   92,  -92,    9,  -57,   42,   72 },
    {   46,   22,  -41,  -33,  -32,  -42,   90,   39,  -62,  -71,  -92,   32,   12,   61,   40,    5 },
    {   76,  -58,  -19,   25,   19,  -42,   19,  -53,   22,   42,  -81,  -28,   41,   54,  -92,    0 }
};

static const int32_t fc2_biases[MODEL_HIDDEN2_DIM] = {
         2,     -3,    -96,    -16,     -9,     41,     34,     87
};

static const int8_t fc3_weights[MODEL_OUTPUT_DIM][MODEL_HIDDEN2_DIM] = {
    {  -41,  -37,   12,  -27,   24, -114,   44,  -76 },
    {  -72,   38,  -49,   62,   45,   16,   10,   17 },
    {   23,  -72,   44,    8,   22,   -9, -127,  -61 },
    { -114,    3,  -63,  -30,   -9,  -46,   92,   32 },
    {   98,   79,   -5,   44,  -72,  -31,   55,  -47 }
};

static const int32_t fc3_biases[MODEL_OUTPUT_DIM] = {
       -70,     55,      5,     58,    -63
};

#endif /* MODEL_DATA_H */
