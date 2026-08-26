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
    0.005358f, /* Accel Std Dev */
    0.005551f, /* Accel Mean Mag */
    2.482842f, /* Mag RMS Delta */
    0.945748f, /* Tilt Angle Delta */
    6.250000f, /* Zero Crossings */
    0.040000f  /* Bout Duration */
};

static const int8_t fc1_weights[MODEL_HIDDEN1_DIM][MODEL_INPUT_DIM] = {
    {  -86,   -3,  -80,  -25,   61,  -29 },
    {  -64,  -65,  -60,   65,   -1,   20 },
    {  -11,   31,   41,   23,   10,  -91 },
    {   26,  -20,   -1, -100,   72,  -49 },
    {    1,   59, -115,   48,   69, -102 },
    {  -37,   12,  -34,  -37,   44,  127 },
    {  -66,  -34,   87,  -22,  -28,   31 },
    {   20,   77,  -12,   40,   25,   20 },
    {  -47,  -97,  -81,  -90,   36,  -68 },
    {    3,   57,   81,   18,  -69,   66 },
    {   60,   16,   23,   73,  -38,   61 },
    {   79,   22,   24,  -23,  -15,    1 },
    {   -7,  -59,   67,   69,   10,   23 },
    {   28,  -50,   90,   24,   46,   45 },
    {  -73,  -76,   35,   73,    6,  -10 },
    {   34,  -91,    2,  -40,   25,   63 }
};

static const int32_t fc1_biases[MODEL_HIDDEN1_DIM] = {
         0,      0,    -20,     -2,    -21,     44,    -11,     12,      0,    -21,    -19,      0,    -30,     13,      0,    -16
};

static const int8_t fc2_weights[MODEL_HIDDEN2_DIM][MODEL_HIDDEN1_DIM] = {
    {   71,   93,    7,   57,   61,  -76,   78,  -20,  -87,  -70,  -35,   12,   39,   96,    1,   43 },
    {   36,   18,   59,   56,  -11,  -71,  -21,   -9,  -61,  -88,  -18,  -82,   -7,   51,  -70,  -35 },
    {  -18,  -89,  -12,  -36,   87, -127,   82,  -22,  -71,    2,   51,  -10,   53,  -75,   36,    2 },
    {   32,  -97,   47,  -61,  -28,  -68,   45,  -86,  -23,  -50,  -60,   92,  -45,   37,   48,   -6 },
    {   33,  -56,   -8,    2,   82,   31,  -39,   75,  -52,  -58,  -75,   72,   -1,  -53,   -8,   23 },
    {  -61,    7,   37,   56,   64,  -89,  -13,  -90,   52,   38,   -4,  -85,   45,   62,  -46,   34 },
    {   65,  -39,   63,   -7,  -79,  -48,  -79,   -4,  -28,  -57,  -68,  -39,   19,   34,  -18,   17 },
    {   57,    5,   68,  -91,  -98,  -37,   86,   75,   -9,   76, -109,   71,  -29,  -93,   36,   59 }
};

static const int32_t fc2_biases[MODEL_HIDDEN2_DIM] = {
         6,      0,    -24,      0,    -19,    -26,      0,    -14
};

static const int8_t fc3_weights[MODEL_OUTPUT_DIM][MODEL_HIDDEN2_DIM] = {
    {   28,   -1, -121,   25,   46,  117,  -57,  123 },
    {   49, -117,   20, -127,    3,  -55,   79,   61 },
    {  -76,  -95, -107,  -26,  -73,    9,   14,  -40 },
    { -127,  -39,   77,   63,   29,   58,  -44,   60 },
    {   37, -124,   17,  120,  -33,  -93,   95,  107 }
};

static const int32_t fc3_biases[MODEL_OUTPUT_DIM] = {
       -35,     62,     63,    -87,      9
};

#endif /* MODEL_DATA_H */
