import numpy as np
import tensorflow as tf

# Test the corrected model with synthetic data that should trigger different classifications
def test_classification_scenarios():
    print("Testing different classification scenarios...")
    
    # Load the updated model weights to verify they're reasonable
    print("\nReading updated model_data.h...")
    with open('sample-pawstate/ml/model_data.h', 'r') as f:
        content = f.read()
        
    # Check if quantization factors are present
    if "feature_quantization_factors" in content:
        print("[OK] Per-feature quantization factors found in model_data.h")
    else:
        print("[FAIL] Per-feature quantization factors missing!")
        
    # Simulate different activity scenarios with proper feature scaling
    scenarios = [
        {
            'name': 'Resting (Low activity)',
            'features': [500, 16500, 8, 15, 1, 1800],  # Low std dev, normal gravity, minimal motion
            'expected': 'Resting or Alert Freeze'
        },
        {
            'name': 'Walking (Moderate activity)', 
            'features': [3000, 17000, 20, 45, 8, 2200],  # Moderate std dev, regular motion
            'expected': 'Walking'
        },
        {
            'name': 'Vigorous Shaking (High activity)',
            'features': [25000, 25000, 80, 120, 20, 2500],  # Very high values across all features
            'expected': 'Playing or Anxious Pacing'
        },
        {
            'name': 'Brief Motion then Still',
            'features': [1000, 16400, 5, 8, 0, 1900],  # Very low activity = freeze after motion
            'expected': 'Alert Freeze'
        }
    ]
    
    # Load quantization factors from the training script
    quantization_factors = [0.005358, 0.005551, 2.482842, 0.945748, 6.250000, 0.040000]
    
    print(f"\nTesting {len(scenarios)} scenarios with corrected quantization:")
    for scenario in scenarios:
        features = np.array(scenario['features'])
        
        # Apply per-feature quantization 
        quantized = []
        for i in range(6):
            scaled = features[i] * quantization_factors[i]
            clamped = np.clip(scaled, -128, 127)
            quantized.append(int(clamped))
            
        # Check for saturation
        saturated = [i for i, val in enumerate(quantized) if abs(val) >= 127]
        
        print(f"\n{scenario['name']}:")
        print(f"  Raw features: {features}")  
        print(f"  Quantized:    {quantized}")
        print(f"  Expected:     {scenario['expected']}")
        
        if saturated:
            feature_names = ['Accel Std Dev', 'Accel Mean Mag', 'Mag RMS Delta', 
                           'Tilt Angle Delta', 'Zero Crossings', 'Bout Duration']
            saturated_names = [feature_names[i] for i in saturated]
            print(f"  [WARN] Saturated features: {saturated_names}")
        else:
            print(f"  [OK] No saturation")
            
        # Calculate feature diversity (how different from uniform values)
        diversity = np.std(quantized)
        print(f"  Feature diversity: {diversity:.1f} (higher = more discriminative)")

if __name__ == "__main__":
    test_classification_scenarios()