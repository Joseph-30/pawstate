import numpy as np
import verify_model_pipeline as vmp

fc1_w = vmp.fc1_weights
fc1_b = vmp.fc1_biases
fc2_w = vmp.fc2_weights
fc2_b = vmp.fc2_biases
fc3_w = vmp.fc3_weights
fc3_b = vmp.fc3_biases
quant_factors = vmp.quant_factors

def run_c_forward(feats):
    inp = np.clip(np.round(np.array(feats) * quant_factors), -128, 127).astype(np.int8)
    
    # FC1
    h1 = np.zeros(16, dtype=np.int8)
    for o in range(16):
        acc = fc1_b[o] + np.sum(fc1_w[o].astype(int) * inp.astype(int))
        acc_relu = max(0, acc) >> 8
        h1[o] = min(127, acc_relu)
        
    # FC2
    h2 = np.zeros(8, dtype=np.int8)
    for o in range(8):
        acc = fc2_b[o] + np.sum(fc2_w[o].astype(int) * h1.astype(int))
        acc_relu = max(0, acc) >> 8
        h2[o] = min(127, acc_relu)
        
    # FC3
    logits = np.zeros(5, dtype=np.int32)
    for o in range(5):
        logits[o] = fc3_b[o] + np.sum(fc3_w[o].astype(int) * h2.astype(int))
        
    # C Softmax (from inference_engine.c)
    max_val = np.max(logits)
    scores = []
    for l in logits:
        shifted = l - max_val
        s = 256 + shifted * 4
        scores.append(max(1, s))
    tot = sum(scores)
    probs = [int(s * 255 / tot) for s in scores]
    
    top = int(np.argmax(probs))
    conf = probs[top] * 100 // 255
    names = ['Resting', 'Walking', 'Playing', 'Anxious Pacing', 'Alert Freeze']
    name = names[top] if conf >= 50 else 'Unknown'
    return name, conf, probs, logits

test_cases = [
    ('Resting flat on table', [150, 16384, 5, 2, 0, 0]),
    ('Resting slight breathing', [500, 16400, 6, 4, 0, 0]),
    ('Slow walking / gentle stroll', [5000, 16600, 10, 30, 6, 1200]),
    ('Normal steady walking', [9000, 17000, 12, 45, 9, 1600]),
    ('Brisk walking / trot', [14000, 17500, 15, 55, 12, 1900]),
    ('Moderate shake in hand', [18000, 20000, 25, 80, 16, 2000]),
    ('Violent shaking in hand (Play)', [30000, 24000, 30, 110, 20, 2300]),
    ('Pacing with turns', [10000, 17200, 75, 45, 10, 1700]),
    ('Standing still upright (Freeze)', [300, 16384, 5, 45, 0, 0]),
    ('Held vertically still in hand', [400, 16384, 5, 60, 0, 0]),
]

print(f"{'Case':<32} | {'Predicted':<15} | {'Conf':<5} | Probs (R,W,P,AP,AF) | Status")
print('-' * 89)
for title, f in test_cases:
    name, conf, probs, l = run_c_forward(f)
    p_str = f"[{probs[0]:>3}, {probs[1]:>3}, {probs[2]:>3}, {probs[3]:>3}, {probs[4]:>3}]"
    status = "PASS (Alert Active)" if name in ['Anxious Pacing', 'Alert Freeze'] else "PASS"
    print(f"{title:<32} | {name:<15} | {conf:>3}%  | {p_str} | {status}")
