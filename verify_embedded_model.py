#!/usr/bin/env python3
"""
Read the embedded model from model_data.h and verify it's working correctly
"""

import numpy as np
import re

# Read model_data.h
with open('sample-pawstate/ml/model_data.h', 'r') as f:
    content = f.read()

def extract_array(content, array_name):
    """Extract a C array into a Python list"""
    # Find the array declaration
    pattern = rf'{array_name}(?:\[[^\]]*\])*\s*=\s*\{{([^;]+)\}};'
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        raise ValueError(f"Could not find {array_name}")
    
    # Extract numbers
    numbers_str = match.group(1)
    numbers = [int(x.strip()) for x in numbers_str.split(',') if x.strip() and not x.strip().startswith('/*')]
    return numbers

def extract_2d_array(content, array_name, rows, cols):
    """Extract a 2D C array"""
    pattern = rf'{array_name}\[[^\]]+\]\[[^\]]+\]\s*=\s*\{{([^;]+)\}};'
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        # Fallback to single close brace
        pattern = rf'{array_name}\[[^\]]+\]\[[^\]]+\]\s*=\s*\{{(.*?)\}};'
        match = re.search(pattern, content, re.DOTALL)
    if not match:
        raise ValueError(f"Could not find {array_name}")
    
    numbers_str = match.group(1)
    # Remove nested braces and extract numbers
    numbers_str = re.sub(r'[{}]', '', numbers_str)
    numbers = [int(x.strip()) for x in numbers_str.split(',') if x.strip() and not x.strip().startswith('/*')]
    
    # Reshape to 2D
    return np.array(numbers).reshape(rows, cols)

print("Loading embedded model from model_data.h...")

# Extract model parameters
fc1_weights = extract_2d_array(content, 'fc1_weights', 16, 6).astype(np.int8)
fc1_biases = np.array(extract_array(content, 'fc1_biases')).astype(np.int32)

fc2_weights = extract_2d_array(content, 'fc2_weights', 8, 16).astype(np.int8)
fc2_biases = np.array(extract_array(content, 'fc2_biases')).astype(np.int32)

fc3_weights = extract_2d_array(content, 'fc3_weights', 5, 8).astype(np.int8)
fc3_biases = np.array(extract_array(content, 'fc3_biases')).astype(np.int32)

feature_quantization_factors = np.array([
    0.005358,  # accel_variance
    0.005551,  # accel_mean_magnitude
    2.482842,  # mag_rms_delta
    0.945748,  # tilt_angle_delta
    6.250000,  # zero_crossing_rate
    0.040000   # activity_bout_dur
])

print(f"Model loaded successfully:")
print(f"  FC1: {fc1_weights.shape} weights, {fc1_biases.shape} biases")
print(f"  FC2: {fc2_weights.shape} weights, {fc2_biases.shape} biases")
print(f"  FC3: {fc3_weights.shape} weights, {fc3_biases.shape} biases")

def relu_requantise(acc):
    """ReLU + requantization (same as C code)"""
    if acc < 0:
        acc = 0
    result = acc >> 8
    return np.clip(result, -128, 127).astype(np.int8)

def softmax_int32(logits):
    """Simplified softmax (same as C code)"""
    max_val = np.max(logits)
    scores = np.maximum(1, 256 + (logits - max_val) * 4)
    total = np.sum(scores)
    if total == 0:
        total = 1
    probs = (scores * 255 / total).astype(np.uint8)
    return probs

def run_inference(features_float):
    """Run INT8 inference exactly like the C code"""
    # Quantize to INT8
    features_quantized = features_float * feature_quantization_factors
    input_int8 = np.clip(features_quantized, -128, 127).astype(np.int8)
    
    # FC1
    hidden1 = np.zeros(16, dtype=np.int8)
    for o in range(16):
        acc = fc1_biases[o]
        for i in range(6):
            acc += int(fc1_weights[o, i]) * int(input_int8[i])
        hidden1[o] = relu_requantise(acc)
    
    # FC2
    hidden2 = np.zeros(8, dtype=np.int8)
    for o in range(8):
        acc = fc2_biases[o]
        for i in range(16):
            acc += int(fc2_weights[o, i]) * int(hidden1[i])
        hidden2[o] = relu_requantise(acc)
    
    # FC3
    logits = np.zeros(5, dtype=np.int32)
    for o in range(5):
        acc = fc3_biases[o]
        for i in range(8):
            acc += int(fc3_weights[o, i]) * int(hidden2[i])
        logits[o] = acc
    
    # Softmax
    probs = softmax_int32(logits)
    
    predicted_class = int(np.argmax(probs))
    confidence = int(int(probs[predicted_class]) * 100 // 255)
    
    return input_int8, logits, probs, predicted_class, confidence

state_names = ['Resting', 'Walking', 'Playing', 'Anxious Pacing', 'Alert Freeze']

print("\n" + "="*80)
print("TESTING WITH DIFFERENT FEATURE VALUES")
print("="*80)

# Test cases that should represent different states
test_cases = [
    ("Resting (low activity)", np.array([50, 1000, 20, 15, 5, 100])),
    ("Walking (moderate)", np.array([200, 1100, 50, 40, 50, 300])),
    ("Playing (high variance)", np.array([1500, 1200, 100, 80, 80, 500])),
    ("Vigorous (very high)", np.array([2000, 1300, 150, 100, 100, 600])),
    ("Stationary (zeros)", np.array([0, 980, 0, 0, 0, 0])),
]

for test_name, features in test_cases:
    input_int8, logits, probs, predicted, confidence = run_inference(features)
    
    print(f"\n{test_name}:")
    print(f"  Float features: {features.astype(int)}")
    print(f"  INT8 quantized: {input_int8}")
    print(f"  Logits: {logits}")
    print(f"  Probabilities: R:{probs[0]} W:{probs[1]} P:{probs[2]} AP:{probs[3]} AF:{probs[4]}")
    print(f"  -> Predicted: {state_names[predicted]} ({confidence}%)")

print("\n" + "="*80 + "\n")
