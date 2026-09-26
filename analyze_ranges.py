import pandas as pd
import numpy as np

# Load full dataset to get real feature ranges
print("Analyzing feature ranges across full dataset...")
df = pd.read_csv('ML_canine_data/df_raw.csv', nrows=500000)

label_map = {
    'sitting': 0,
    'lying down': 0,
    'walking': 1,
    'body shake': 2,
    'standing': 4
}
df['class_id'] = df['Position'].map(label_map)
walking_idx = df[df['class_id'] == 1].index
np.random.seed(42)
pacing_idx = np.random.choice(walking_idx, size=int(len(walking_idx)*0.3), replace=False)
df.loc[pacing_idx, 'class_id'] = 3
df = df.dropna(subset=['class_id'])

# Scale data
df['ax'] = df['Neck.Acc.X'] * 16384
df['ay'] = df['Neck.Acc.Y'] * 16384
df['az'] = df['Neck.Acc.Z'] * 16384
df['mx'] = df['Neck.Mag.X'] * 10
df['my'] = df['Neck.Mag.Y'] * 10
df['mz'] = df['Neck.Mag.Z'] * 10
df = df.iloc[::2].reset_index(drop=True)

# Extract features for more windows
WINDOW_SIZE = 125
features = []
labels = []

print("Extracting features from 1000 windows...")
for i in range(0, min(50000, len(df) - WINDOW_SIZE), 50):  # Sample every 50 for speed
    window = df.iloc[i:i+WINDOW_SIZE]
    cls = int(window['class_id'].mode()[0])
    
    ax = window['ax'].values
    ay = window['ay'].values
    az = window['az'].values
    mx = window['mx'].values
    my = window['my'].values
    mz = window['mz'].values
    
    # Feature 1: Accel Std Dev
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
    
    # Feature 4: Tilt Angle Delta
    pitch = np.degrees(np.arctan2(ax, np.sqrt(ay**2 + az**2)))
    f4 = np.max(pitch) - np.min(pitch)
    
    # Feature 5: Zero Crossing Rate
    hysteresis = 1000
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
    
    # Feature 6: Bout Duration
    dev = np.abs(mags**2 - 16384**2)
    active_samples = np.sum(dev > (1600**2))
    f6 = active_samples * 20
    
    features.append([f1, f2, f3, f4, f5, f6])
    labels.append(cls)

X = np.array(features, dtype=np.float32)
y = np.array(labels, dtype=np.int32)

print(f"\nAnalyzed {len(X)} windows across all classes")
unique, counts = np.unique(y, return_counts=True)
for cls, count in zip(unique, counts):
    class_names = ['Resting', 'Walking', 'Playing', 'Anxious Pacing', 'Alert Freeze']
    print(f"  {cls} ({class_names[cls]}): {count} windows")

print(f"\nFeature statistics:")
feature_names = ['Accel Std Dev', 'Accel Mean Mag', 'Mag RMS Delta', 'Tilt Angle Delta', 'Zero Crossings', 'Bout Duration']
for i in range(6):
    print(f"\n{feature_names[i]}:")
    print(f"  Min: {X[:,i].min():.1f}")
    print(f"  Max: {X[:,i].max():.1f}")
    print(f"  Mean: {X[:,i].mean():.1f}")
    print(f"  Std: {X[:,i].std():.1f}")
    print(f"  95th percentile: {np.percentile(X[:,i], 95):.1f}")
    print(f"  99th percentile: {np.percentile(X[:,i], 99):.1f}")

# Calculate optimal quantization factors for each feature
print(f"\nOptimal quantization factors (to use ~80% of INT8 range):")
for i in range(6):
    # Use 95th percentile to avoid outliers
    max_val = np.percentile(X[:,i], 95)
    target_int8 = 100  # Use 100 of 127 max range
    optimal_factor = target_int8 / max_val if max_val > 0 else 1.0
    print(f"  {feature_names[i]}: factor = {optimal_factor:.6f} (current: 16.0)")
    
# Test different quantization approaches
print(f"\n=== Testing different quantization approaches ===")

# Approach 1: Per-feature scaling to 95th percentile
print(f"\nApproach 1: Per-feature scaling to 95th percentile")
X_scaled = np.zeros_like(X)
for i in range(6):
    max_val = np.percentile(X[:,i], 95)
    scale = 100.0 / max_val if max_val > 0 else 1.0
    X_scaled[:,i] = X[:,i] * scale
    X_scaled[:,i] = np.clip(X_scaled[:,i], -128, 127)
    
    saturated = np.sum((X_scaled[:,i] == 127) | (X_scaled[:,i] == -128))
    print(f"  {feature_names[i]}: {saturated}/{len(X)} samples saturated ({saturated/len(X)*100:.1f}%)")

# Approach 2: Global scaling with smaller factor
print(f"\nApproach 2: Global scaling with factor 0.1")
X_scaled2 = np.clip(X * 0.1, -128, 127)
for i in range(6):
    saturated = np.sum((X_scaled2[:,i] == 127) | (X_scaled2[:,i] == -128))
    print(f"  {feature_names[i]}: {saturated}/{len(X)} samples saturated ({saturated/len(X)*100:.1f}%)")
    print(f"    Range: {X_scaled2[:,i].min():.1f} to {X_scaled2[:,i].max():.1f}")