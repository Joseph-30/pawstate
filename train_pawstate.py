import pandas as pd
import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os

# 1. Load Data
print("Loading data...")
# Read a subset for fast execution (500k rows = 5000 seconds = ~1.3 hours of data at 100Hz)
df = pd.read_csv('ML_canine_data/df_raw.csv', nrows=500000)

# 2. Map Labels
# We map Mendeley labels to our 5 classes
# 0: Resting, 1: Walking, 2: Playing, 3: Anxious Pacing, 4: Alert Freeze
label_map = {
    'sitting': 0,
    'lying down': 0,  # Fixed: was 'lying' -> should be 'lying down'
    'walking': 1,
    'body shake': 2, # Active / Playing
    'standing': 4    # Static standing -> Freeze
}
df['class_id'] = df['Position'].map(label_map)

# Synthesize "Anxious Pacing" (3) by finding walking segments and randomly assigning some to 3
# In reality we'd want actual pacing data, but for this integration we synthesize it.
walking_idx = df[df['class_id'] == 1].index
# Set roughly 30% of walking to pacing
np.random.seed(42)
pacing_idx = np.random.choice(walking_idx, size=int(len(walking_idx)*0.3), replace=False)
df.loc[pacing_idx, 'class_id'] = 3

# Drop unmapped or NaN
df = df.dropna(subset=['class_id'])

# 3. Scale and Downsample
# Dataset is in 'g' for accel. Our C code uses 1g = 16384 LSB.
# We multiply Accel by 16384. Mag is in uT, C code uses raw mag. Let's multiply mag by 10 for scale.
df['ax'] = df['Neck.Acc.X'] * 16384
df['ay'] = df['Neck.Acc.Y'] * 16384
df['az'] = df['Neck.Acc.Z'] * 16384
df['mx'] = df['Neck.Mag.X'] * 10
df['my'] = df['Neck.Mag.Y'] * 10
df['mz'] = df['Neck.Mag.Z'] * 10

# Downsample from 100Hz to 50Hz (take every 2nd row)
df = df.iloc[::2].reset_index(drop=True)

# 4. Feature Extraction (Windowing)
WINDOW_SIZE = 125
features = []
labels = []

print("Extracting features...")
for i in range(0, len(df) - WINDOW_SIZE, WINDOW_SIZE//2): # 50% overlap
    window = df.iloc[i:i+WINDOW_SIZE]
    
    # Check if window spans multiple classes; take the mode
    cls = int(window['class_id'].mode()[0])
    
    ax = window['ax'].values
    ay = window['ay'].values
    az = window['az'].values
    mx = window['mx'].values
    my = window['my'].values
    mz = window['mz'].values
    
    # Feature 1: Accel Std Dev (Sum of per-axis std dev, since we fixed variance to stddev)
    std_x = np.std(ax)
    std_y = np.std(ay)
    std_z = np.std(az)
    f1 = std_x + std_y + std_z
    
    # Feature 2: Accel Mean Mag
    mags = np.sqrt(ax**2 + ay**2 + az**2)
    f2 = np.mean(mags)
    
    # Feature 3: Mag RMS Delta
    dmx = np.diff(mx)
    dmy = np.diff(my)
    dmz = np.diff(mz)
    mag_deltas = np.sqrt(dmx**2 + dmy**2 + dmz**2)
    f3 = np.sqrt(np.mean(mag_deltas**2)) if len(mag_deltas) > 0 else 0
    
    # Feature 4: Tilt Angle Delta (Pitch)
    # pitch = atan2(ax, sqrt(ay^2 + az^2))
    pitch = np.degrees(np.arctan2(ax, np.sqrt(ay**2 + az**2)))
    f4 = np.max(pitch) - np.min(pitch)
    
    # Feature 5: Zero Crossing Rate (X-axis)
    hysteresis = 1000 # mg threshold roughly
    crossings = 0
    state = 1 if ax[0] >= hysteresis else (-1 if ax[0] <= -hysteresis else 0)
    for val in ax[1:]:
        if state >= 0 and val <= -hysteresis:
            crossings += 1
            state = -1
        elif state <= 0 and val >= hysteresis:
            crossings += 1
            state = 1
    f5 = crossings
    
    # Feature 6: Bout Duration (ms)
    # Threshold = 100 mg -> 1600 LSB. gravity=16384
    dev = np.abs(mags**2 - 16384**2)
    active_samples = np.sum(dev > (1600**2))
    f6 = active_samples * 20 # 20ms per sample
    
    features.append([f1, f2, f3, f4, f5, f6])
    labels.append(cls)

X = np.array(features, dtype=np.float32)
y = np.array(labels, dtype=np.int32)

print(f"Extracted {len(X)} windows.")

# 5. Quantize Inputs to INT8 with per-feature scaling
# Calculate optimal quantization factors for each feature based on 95th percentile
print("Calculating optimal quantization factors...")
quantization_factors = []
for i in range(6):
    max_val = np.percentile(X[:, i], 95)
    # Use 100 out of 127 max range to avoid saturation
    optimal_factor = 100.0 / max_val if max_val > 0 else 1.0
    quantization_factors.append(optimal_factor)
    feature_names = ['Accel Std Dev', 'Accel Mean Mag', 'Mag RMS Delta', 
                    'Tilt Angle Delta', 'Zero Crossings', 'Bout Duration']
    print(f"  {feature_names[i]}: factor = {optimal_factor:.6f}")

# Apply per-feature quantization
X_quant = np.zeros_like(X)
for i in range(6):
    X_quant[:, i] = X[:, i] * quantization_factors[i]
    X_quant[:, i] = np.clip(X_quant[:, i], -128, 127)

# Check saturation
print("\nSaturation check:")
for i in range(6):
    feature_names = ['Accel Std Dev', 'Accel Mean Mag', 'Mag RMS Delta', 
                    'Tilt Angle Delta', 'Zero Crossings', 'Bout Duration']
    saturated = np.sum((X_quant[:,i] == 127) | (X_quant[:,i] == -128))
    print(f"  {feature_names[i]}: {saturated}/{len(X)} saturated ({saturated/len(X)*100:.1f}%)")

X_quant = X_quant.astype(np.float32)

# 6. Train Keras Model with Class Balancing
print("Training model...")

# Calculate class weights to balance the training
from sklearn.utils.class_weight import compute_class_weight
classes = np.unique(y)
class_weights_array = compute_class_weight('balanced', classes=classes, y=y)
class_weights = {int(cls): float(weight) for cls, weight in zip(classes, class_weights_array)}

print(f"\nClass distribution before balancing:")
unique, counts = np.unique(y, return_counts=True)
for cls, count in zip(unique, counts):
    print(f"  Class {cls}: {count} samples ({count/len(y)*100:.1f}%)")

print(f"\nClass weights for balanced training:")
for cls, weight in class_weights.items():
    print(f"  Class {cls}: {weight:.2f}x")

model = keras.Sequential([
    keras.Input(shape=(6,)),
    layers.Dense(16, activation='relu'),
    layers.Dense(8, activation='relu'),
    layers.Dense(5, activation='linear') # Softmax done in C code
])

model.compile(optimizer='adam',
              loss=keras.losses.SparseCategoricalCrossentropy(from_logits=True),
              metrics=['accuracy'])

model.fit(X_quant, y, 
          epochs=20,  # Increased epochs for better convergence
          batch_size=32, 
          validation_split=0.2,
          class_weight=class_weights)  # Apply class weights!

# 7. Extract weights and quantize them for C engine
print("Quantizing weights to INT8...")
def quantize_weights(w):
    # Standard symmetric int8 quantization
    max_abs = np.max(np.abs(w))
    if max_abs == 0: scale = 1.0
    else: scale = 127.0 / max_abs
    w_q = np.clip(np.round(w * scale), -127, 127).astype(np.int8)
    return w_q, scale

layers_data = []
S_in = 1.0  # Input to Layer 1 is already scaled to int8 range

for i, layer in enumerate(model.layers):
    w, b = layer.get_weights()
    max_w = np.max(np.abs(w))
    if max_w == 0: max_w = 1.0
    
    if i < 2:
        # Layers 1 and 2 (with >> 8 shift in C)
        # We want W_c = w * (S_out / S_in) * 256
        # And max(abs(W_c)) = 127
        # So 127 = max_w * (S_out / S_in) * 256
        # S_out = 127 * S_in / (max_w * 256)
        S_out = 127.0 * S_in / (max_w * 256.0)
        
        # W_c = w * (S_out / S_in) * 256 = w * 127 / max_w
        w_int8 = np.clip(np.round(w * 127.0 / max_w), -127, 127).astype(np.int8)
        
        # B_c = b * S_out * 256
        b_int32 = np.round(b * S_out * 256.0).astype(np.int32)
    else:
        # Layer 3 (no >> 8 shift in C)
        # We want W_c = w * (S_out / S_in)
        # 127 = max_w * (S_out / S_in)
        # S_out = 127 * S_in / max_w
        S_out = 127.0 * S_in / max_w
        
        w_int8 = np.clip(np.round(w * 127.0 / max_w), -127, 127).astype(np.int8)
        
        # B_c = b * S_out
        b_int32 = np.round(b * S_out).astype(np.int32)

    layers_data.append((w_int8, b_int32))
    S_in = S_out

# 8. Generate C Header
print("Generating model_data.h...")
fc1_w, fc1_b = layers_data[0]
fc2_w, fc2_b = layers_data[1]
fc3_w, fc3_b = layers_data[2]

def format_matrix(m):
    # Keras is (in, out). C code is (out, in)
    m = m.T 
    lines = []
    for row in m:
        lines.append("    { " + ", ".join(f"{x:4d}" for x in row) + " }")
    return ",\n".join(lines)

def format_bias(b):
    return "    " + ", ".join(f"{x:6d}" for x in b)

c_header = f"""#ifndef MODEL_DATA_H
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
static const float feature_quantization_factors[MODEL_INPUT_DIM] = {{
    {quantization_factors[0]:.6f}f, /* Accel Std Dev */
    {quantization_factors[1]:.6f}f, /* Accel Mean Mag */
    {quantization_factors[2]:.6f}f, /* Mag RMS Delta */
    {quantization_factors[3]:.6f}f, /* Tilt Angle Delta */
    {quantization_factors[4]:.6f}f, /* Zero Crossings */
    {quantization_factors[5]:.6f}f  /* Bout Duration */
}};

static const int8_t fc1_weights[MODEL_HIDDEN1_DIM][MODEL_INPUT_DIM] = {{
{format_matrix(fc1_w)}
}};

static const int32_t fc1_biases[MODEL_HIDDEN1_DIM] = {{
{format_bias(fc1_b)}
}};

static const int8_t fc2_weights[MODEL_HIDDEN2_DIM][MODEL_HIDDEN1_DIM] = {{
{format_matrix(fc2_w)}
}};

static const int32_t fc2_biases[MODEL_HIDDEN2_DIM] = {{
{format_bias(fc2_b)}
}};

static const int8_t fc3_weights[MODEL_OUTPUT_DIM][MODEL_HIDDEN2_DIM] = {{
{format_matrix(fc3_w)}
}};

static const int32_t fc3_biases[MODEL_OUTPUT_DIM] = {{
{format_bias(fc3_b)}
}};

#endif /* MODEL_DATA_H */
"""

with open('sample-pawstate/ml/model_data.h', 'w') as f:
    f.write(c_header)

print("Done! model_data.h updated.")
