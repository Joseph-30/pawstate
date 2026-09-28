# PawState — Real-Time Canine Emotion & Activity Monitoring

> **TRON Programming Contest 2026 Entry**
> μT-Kernel 3.0 on BBC micro:bit v2 (nRF52833)

## Overview

PawState is a collar-mounted embedded system that classifies a dog's emotional and behavioural state in real-time using IMU sensor data, a TinyML neural network classifier, and BLE notifications to a smartphone — all orchestrated by the μT-Kernel 3.0 real-time operating system.

### Behavioural States Classified

| State | Description | LED Pattern |
| ------- | ------------- | ------------- |
| **Resting** | Lying down or sitting still | Zzz sleep symbol |
| **Walking** | Steady locomotion, regular gait | Right arrow |
| **Playing** | High-energy irregular motion | Star |
| **Anxious Pacing** | Rhythmic repetitive movement (stress) | Exclamation mark |
| **Alert Freeze** | Sudden stillness after motion (fear) | Square |

### Key Technical Features

- **50 Hz IMU Sampling** — 6-axis data (3 accel + 3 magnetometer) from LSM303AGR
- **4 Prioritised RTOS Tasks** — Hard real-time guarantees via μT-Kernel 3.0
- **INT8 Quantised Neural Network** — ~290 bytes, <50ms inference on Cortex-M4
- **High-Priority Anxiety Alert Path** — BLE notification within ~30ms
- **Offline State Buffering** — 24h of state history when BLE is disconnected
- **On-board Speaker Alerts** — Audible beep pattern for anxiety spikes
- **5×5 LED Status Display** — Real-time behavioural state visualisation

---

## Hardware Platform

### BBC micro:bit v2 Specifications

| Component | Detail |
| ----------- | -------- |
| SoC | Nordic nRF52833 (ARM Cortex-M4F @ 64 MHz) |
| Flash | 512 KB |
| RAM | 128 KB |
| Accelerometer | LSM303AGR (I2C addr: 0x19) |
| Magnetometer | LSM303AGR (I2C addr: 0x1E) |
| Internal I2C | SCL: P0.08, SDA: P0.16 |
| BLE | Bluetooth 5.0 (via SoftDevice S140) |
| LED Matrix | 5×5 multiplexed (10 GPIO pins) |
| Speaker | On-board magnetic (P0.00, PWM) |
| Buttons | A (P0.14), B (P0.23) |
| Temperature | On-chip sensor (0.25°C resolution) |

### Sensor Note

The micro:bit v2's LSM303AGR provides **accelerometer + magnetometer** (not a gyroscope). The magnetometer's rate-of-change serves as a rotational velocity proxy, which is sufficient for the 5 behavioural classes in collar-mounted orientation.

---

## Project Structure

```
sample-pawstate/
├── app/                          # Application tasks (μT-Kernel 3.0)
│   ├── pawstate_main.c/.h       # Entry point, kernel object creation
│   ├── imu_sampler.c/.h         # Task 1: 50Hz IMU sampling (Priority 1)
│   ├── feature_extractor.c/.h   # Task 2: Feature extraction (Priority 5)
│   ├── tinyml_classifier.c/.h   # Task 3: ML inference (Priority 10)
│   └── ble_logger.c/.h          # Task 4: BLE notifications (Priority 15)
├── drivers/                      # Hardware drivers
│   ├── drv_i2c.c/.h             # nRF52833 TWIM0 I2C master
│   ├── drv_lsm303agr.c/.h      # LSM303AGR accel/mag sensor
│   ├── drv_led_matrix.c/.h     # 5×5 LED display
│   ├── drv_gpio.c/.h            # Buttons, speaker, temperature
│   └── drv_ble.c/.h             # BLE GATT abstraction
├── ml/                           # Machine learning
│   ├── model_data.h             # INT8 neural network weights
│   └── inference_engine.c/.h    # Pure C inference engine
├── util/                         # Utility modules
│   ├── circular_buffer.c/.h     # Lock-free SPSC ring buffer
│   └── math_utils.c/.h          # Fixed-point Q16.16 math
├── include/                      # Shared headers
│   ├── pawstate_config.h        # Global configuration constants
│   ├── pawstate_types.h         # Shared data types
│   └── nrf52833_hal.h           # nRF52833 register definitions
└── README.md                    # This file
```

---

## RTOS Architecture

### Task Pipeline

```
┌──────────────────┐     Lock-free      ┌──────────────────┐
│   IMU Sampler    │────  SPSC Buffer ──▶│ Feature Extractor│
│   Priority 1     │   EVT_NEW_SAMPLES   │   Priority 5     │
│   50 Hz / 20ms   │                     │   Every 2.5s     │
└──────────────────┘                     └────────┬─────────┘
                                                  │ EVT_FEATURES_READY
                                                  │ + Semaphore
                                                  ▼
                                         ┌──────────────────┐
                                         │ TinyML Classifier│
                                         │   Priority 10    │
                                         │   On-demand      │
                                         └────────┬─────────┘
                                                  │ Message Buffer
                                                  │ + EVT_ANXIETY_SPIKE ⚡
                                                  ▼
                                         ┌──────────────────┐
                                         │  BLE Event Logger│
                                         │   Priority 15    │
                                         │   Event-driven   │
                                         └──────────────────┘
```

### μT-Kernel 3.0 Kernel Objects Used

| Object | Type | Purpose |
| -------- | ------ | --------- |
| `sem_i2c` | Semaphore | I2C bus mutual exclusion |
| `sem_feature_buf` | Semaphore | Feature vector buffer protection |
| `flg_pipeline` | Event Flag | Inter-task signaling (5 flag bits) |
| `mbf_ble_events` | Message Buffer | Classifier → BLE Logger event queue |
| `cyc_imu_sample` | Cyclic Handler | 50 Hz IMU sampling trigger |
| `cyc_led_refresh` | Cyclic Handler | 120 Hz LED matrix refresh |

### Anxiety Spike Alert Path

When the classifier detects a transition to `ANXIOUS_PACING` or `ALERT_FREEZE` with confidence ≥55%, it sends the immediate anxiety alert path. Ordinary non-anxiety states at 50-69% confidence remain subject to temporal debouncing; confidence ≥70% switches the displayed state immediately:

1. **EVT_ANXIETY_SPIKE** event flag is set → wakes BLE Logger
2. **Message buffer** receives high-priority event with `is_anxiety=1`
3. **Speaker** plays triple-beep alert pattern (2 kHz, 200ms each)
4. **LED matrix** shows exclamation mark (anxious) or square (freeze)
5. **BLE notification** sent on dedicated anxiety characteristic
6. Total latency: **< 30ms** from classification to BLE push

### Verifying Actions via Serial Console

If you do not have a dog (or want to test on a desk), you can verify the entire workflow and state transitions via the serial console (T-Monitor).

1. Connect the micro:bit to your PC via USB.
2. Open a Serial Terminal program (e.g., PuTTY, Tera Term, or the Arduino Serial Monitor).
3. Connect to the micro:bit's COM port with the following settings:
   - **Baud Rate**: 115200
   - **Data Bits**: 8
   - **Parity**: None
   - **Stop Bits**: 1

You will see `tm_printf` log messages dynamically printing out the internal state. For example:

- **Normal state change**: `[ML] State changed to: Walking (Confidence: 85%)`
- **Anxiety alert**: `[ML] *** ANXIETY SPIKE DETECTED! Triggering Alarm ***`

**To test the Alert Freeze (Square):** Simply leave the board completely still on a flat surface.
**To test Anxiety Pacing / Playing:** Vigorously shake the micro:bit back and forth. You should see the LED matrix flash an exclamation mark, hear the speaker beep, and see the `ANXIETY SPIKE` log in the console.

---

## Integration with μT-Kernel 3.0 Build System

### Prerequisites

1. **μT-Kernel 3.0 source** — Clone from [tron-forum/mtkernel_3](https://github.com/tron-forum/mtkernel_3) (v3.00.07)
2. **BSP2** — Clone from [tron-forum/mtk3_bsp2](https://github.com/tron-forum/mtk3_bsp2) for nRF52833 support
3. **GNU Arm Embedded Toolchain** — `arm-none-eabi-gcc` (version 12.x or later)
4. **Make** — GNU Make 4.x

### Directory Layout (Expected)

```
project_root/
├── mtkernel_3/              # μT-Kernel 3.0 kernel source (git clone)
│   ├── include/             # Kernel headers (tk/tkernel.h, etc.)
│   ├── kernel/              # Kernel source
│   └── ...
├── mtk3_bsp2/               # BSP2 for nRF52833 (git clone)
│   ├── sysdepend/nrf5/      # nRF52833 board support
│   └── ...
└── sample-pawstate/         # ← THIS PROJECT (your application)
    ├── app/
    ├── drivers/
    ├── ml/
    ├── util/
    └── include/
```

### Adding PawState to an Existing Makefile Build

Add the following source files to your application's Makefile:

```makefile
# PawState Application Sources
APP_SRCS += \
    $(APP_DIR)/app/pawstate_main.c \
    $(APP_DIR)/app/imu_sampler.c \
    $(APP_DIR)/app/feature_extractor.c \
    $(APP_DIR)/app/tinyml_classifier.c \
    $(APP_DIR)/app/ble_logger.c

# PawState Driver Sources
APP_SRCS += \
    $(APP_DIR)/drivers/drv_i2c.c \
    $(APP_DIR)/drivers/drv_lsm303agr.c \
    $(APP_DIR)/drivers/drv_led_matrix.c \
    $(APP_DIR)/drivers/drv_gpio.c \
    $(APP_DIR)/drivers/drv_ble.c

# PawState ML Sources
APP_SRCS += \
    $(APP_DIR)/ml/inference_engine.c

# PawState Utility Sources
APP_SRCS += \
    $(APP_DIR)/util/circular_buffer.c \
    $(APP_DIR)/util/math_utils.c

# Include Paths
CFLAGS += -I$(APP_DIR)/include
CFLAGS += -I$(APP_DIR)/app
CFLAGS += -I$(APP_DIR)/drivers
CFLAGS += -I$(APP_DIR)/ml
CFLAGS += -I$(APP_DIR)/util
```

### Compiler Flags

```makefile
# Target: ARM Cortex-M4F (nRF52833)
CFLAGS += -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16

# Optimisation
CFLAGS += -Os -ffunction-sections -fdata-sections

# Warnings
CFLAGS += -Wall -Wextra -Werror

# BLE SoftDevice (set to 1 when SoftDevice is linked)
CFLAGS += -DBLE_SOFTDEVICE_PRESENT=0

# Linker: remove unused sections
LDFLAGS += -Wl,--gc-sections
```

### Linker Script Modifications

If using SoftDevice S140 v7.3.0, modify the linker script to reserve memory:

```ld
/* Without SoftDevice */
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K

/* With SoftDevice S140 v7.3.0 */
FLASH (rx)  : ORIGIN = 0x00027000, LENGTH = 356K
RAM   (rwx) : ORIGIN = 0x20002000, LENGTH = 120K
```

### Build & Flash

For this checkout, use the root [`README.md`](../../../README.md). The checked-in Makefile compiles this application from `mtk3/mtkernel_3/build_make`; it does not create `build/pawstate.hex`.

```bash
# Build the firmware
make all

# Generate the flashable file from the ELF in the build directory
arm-none-eabi-objcopy -O ihex mtkernel_3.elf mtkernel_3.hex

# Or copy the generated .hex file to the MICROBIT USB drive
cp mtkernel_3.hex /media/MICROBIT/
```

---

## Replacing Placeholder Model Weights with Trained Weights

The checked-in `ml/model_data.h` is generated model data. To replace it with a retrained model, run `python train_pawstate.py` from the repository root; the script writes both the canonical build copy and the inspection mirror.

### Step 1: Train the Model (Python)

```python
import tensorflow as tf
import numpy as np

# Define the same architecture
model = tf.keras.Sequential([
    tf.keras.layers.Dense(16, activation='relu', input_shape=(6,)),
    tf.keras.layers.Dense(8, activation='relu'),
    tf.keras.layers.Dense(5, activation='softmax')
])

model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

# Train with your labelled IMU dataset
# X_train shape: (N, 6) — feature vectors
# y_train shape: (N,) — labels 0-4
model.fit(X_train, y_train, epochs=50, validation_split=0.2)
```

### Step 2: Quantise to INT8

```python
# Full integer quantisation
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

# Provide representative dataset for calibration
def representative_dataset():
    for i in range(100):
        yield [X_train[i:i+1].astype(np.float32)]

converter.representative_dataset = representative_dataset
tflite_model = converter.convert()

# Save
with open('pawstate_model.tflite', 'wb') as f:
    f.write(tflite_model)
```

### Step 3: Extract Weights and Update `model_data.h`

```python
# Load the quantised model
interpreter = tf.lite.Interpreter(model_content=tflite_model)
interpreter.allocate_tensors()

# Get tensor details
for detail in interpreter.get_tensor_details():
    print(f"Name: {detail['name']}")
    print(f"Shape: {detail['shape']}")
    print(f"Dtype: {detail['dtype']}")
    print(f"Quantization: {detail['quantization_parameters']}")
    tensor_data = interpreter.get_tensor(detail['index'])
    print(f"Data: {tensor_data.flatten()[:10]}...")
    print()
```

Use the extracted weight arrays to replace `fc1_weights`, `fc2_weights`, `fc3_weights` and their corresponding biases in `ml/model_data.h`. Update the quantisation scale and zero-point constants as well.

### Step 4: Validate Accuracy

```python
# Test quantised model accuracy
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

correct = 0
for i in range(len(X_test)):
    # Quantise input
    input_scale, input_zp = input_details[0]['quantization']
    x_quant = (X_test[i] / input_scale + input_zp).astype(np.int8)

    interpreter.set_tensor(input_details[0]['index'], x_quant.reshape(1, 6))
    interpreter.invoke()

    output = interpreter.get_tensor(output_details[0]['index'])
    predicted = np.argmax(output)
    if predicted == y_test[i]:
        correct += 1

accuracy = correct / len(X_test)
print(f"Quantised model accuracy: {accuracy:.2%}")
# Target: >80% F1-score
```

---

## Configuration Reference

All tuneable parameters are in `include/pawstate_config.h`:

| Parameter | Value | Description |
| ----------- | ------- | ------------- |
| `IMU_SAMPLE_RATE_HZ` | 50 | IMU sampling frequency |
| `FEATURE_WINDOW_SIZE` | 125 | Sliding window (2.5s at 50 Hz) |
| `FEATURE_VECTOR_DIM` | 6 | Number of extracted features |
| `NUM_BEHAVIOUR_CLASSES` | 5 | Classification output classes |
| `ANXIETY_SPIKE_CONFIDENCE` | 55% | Threshold for anxiety alerts; ordinary state changes use a 70% immediate-switch threshold |
| `CIRC_BUFFER_CAPACITY` | 256 | IMU sample ring buffer size |
| `TASK_PRI_IMU_SAMPLER` | 1 | Highest priority |
| `TASK_PRI_BLE_LOGGER` | 15 | Lowest priority |
| `LED_REFRESH_RATE_HZ` | 120 | LED matrix scan rate |
| `SPEAKER_ALERT_FREQ_HZ` | 2000 | Anxiety beep frequency |

---

## Dependencies

| Dependency | Version | Source | Purpose |
| ----------- | --------- | -------- | --------- |
| μT-Kernel 3.0 | v3.00.07 | [tron-forum/mtkernel_3](https://github.com/tron-forum/mtkernel_3) | RTOS kernel |
| mtk3_bsp2 | latest | [tron-forum/mtk3_bsp2](https://github.com/tron-forum/mtk3_bsp2) | nRF52833 board support |
| GNU Arm Toolchain | 12.x+ | [developer.arm.com](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/downloads) | Cross-compiler |
| SoftDevice S140 | v7.3.0 | [nordicsemi.com](https://www.nordicsemi.com/Software-and-tools/Software/S140) | BLE stack (optional for initial testing) |

> **Note**: No external libraries are required beyond the μT-Kernel 3.0 kernel and BSP. The inference engine, drivers, and utilities are all self-contained.

---

## Team

| Member | Role | Responsibility |
| -------- | ------ | ---------------- |
| **Joseph P George** | CEO/CTO, Team Lead | RTOS firmware, system architecture |
| **Kasinath Salim** | Head of AI/ML | Model training, signal processing |
| **Evan George Varghese** | Head of Product/Hardware | Hardware integration, mobile app |

---

## License

This project is developed for the TRON Programming Contest 2026.
μT-Kernel 3.0 is provided under T-License 2.2.

© 2026 PawState Team. All rights reserved.
