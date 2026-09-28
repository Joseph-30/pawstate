# PawState: Real-Time Canine Welfare & Anxiety Monitoring System
### Built on BBC micro:bit v2 (nRF52833 Cortex-M4F) and μT-Kernel 3.0 RTOS
**Submission for the TRON Programming Contest 2026**

[![License](https://img.shields.io/badge/license-T--License-blue.svg)](https://www.tron.org/)
[![RTOS](https://img.shields.io/badge/RTOS-%CE%BCT--Kernel%203.0-brightgreen.svg)](https://github.com/tron-forum/mtkernel_3)
[![Platform](https://img.shields.io/badge/Hardware-BBC%20micro%3Abit%20v2-red.svg)](https://microbit.org/)
[![SoC](https://img.shields.io/badge/SoC-Nordic%20nRF52833%20Cortex--M4F-orange.svg)](https://www.nordicsemi.com/)
[![Verification](https://img.shields.io/badge/Verification-100%25%20PASS-success.svg)](verify_model_pipeline.py)

---

## 1. Executive Summary

**PawState** is an intelligent, collar-mounted wearable device engineered to detect and alert on canine emotional stress, panic spikes, and daily behavioral activities in real time. Operating entirely at the edge without cloud latency, PawState executes high-frequency inertial sensor sampling, 6-dimensional fixed-point biomechanical feature extraction, and an ultra-compact **INT8 Quantized Neural Network** directly on the **BBC micro:bit v2** powered by **μT-Kernel 3.0**.

### Key Technical Achievements
- **Zero Cloud Latency:** All sensor sampling, feature extraction, and ML inference execute locally in micro-watts on the collar.
- **Hard Real-Time Architecture:** Strict 4-tier μT-Kernel 3.0 task priority model ensures 50 Hz IMU sensor sampling is never interrupted or delayed by heavy mathematical inference or wireless operations.
- **Fast 1.24s Response Time:** A 50% overlapping sliding window (`FEATURE_STEP_SIZE = 62` @ 50 Hz) cuts latency by **50%** while preserving the 2.5-second observation window needed for stride cadence.
- **Multi-Modal Alerting:** Immediate on-collar audio-visual feedback (5×5 LED matrix + 2 kHz acoustic buzzer) paired with Bluetooth Low Energy (BLE) mobile push notifications and offline ring buffer sync.
- **Ultralight Edge Footprint:** Custom pure C INT8 feedforward engine requires **< 500 bytes code** and **290 bytes weights**, consuming < 10% of available Flash and RAM.

---

## 2. Official Submission Materials & Documentation

As required for evaluation and execution, comprehensive manuals and architectural specifications are provided in the [`docs/`](docs/) directory:

| Document | File Link | Purpose & Contents |
| :--- | :--- | :--- |
| **Operation Manual** | [`docs/OPERATION_MANUAL.md`](docs/OPERATION_MANUAL.md) | Comprehensive user manual: collar mounting orientation, battery vs USB power modes, 5×5 LED icon guide, acoustic alarm behavior, mobile and web dashboard operation, offline buffering, and troubleshooting. |
| **Procedure Manual** | [`docs/PROCEDURE_MANUAL.md`](docs/PROCEDURE_MANUAL.md) | Step-by-step evaluation procedure for contest judges: fast-track flashing (<2 min), physical hand demonstration protocol for all 5 behaviors, automated verification suites, building from source, and evaluation checklist. |
| **System Architecture** | [`docs/SYSTEM_ARCHITECTURE_AND_DESIGN.md`](docs/SYSTEM_ARCHITECTURE_AND_DESIGN.md) | In-depth engineering specification: μT-Kernel 3.0 task priorities, IPC primitives, Q16.16 feature engine, 1.24s sliding window mechanics, INT8 network quantization, and memory map. |

---

## 3. Fast-Track Evaluation (Under 2 Minutes)

Judges can evaluate PawState immediately without installing any embedded toolchains:

### Step 1: Flash Pre-Compiled Firmware
1. Plug a BBC micro:bit v2 into your PC via USB (it appears as a USB drive named `MICROBIT`).
2. Drag and drop [`mtkernel_3.hex`](mtkernel_3.hex) onto the `MICROBIT` drive.
3. The board flashes in seconds and boots into **μT-Kernel 3.0**.

### Step 2: Physical Hand Demonstration Protocol
Test the physical motions with the micro:bit in your hand:
- **Resting (Z):** Place flat on desk $\rightarrow$ Displays **`Z`** (Silent).
- **Walking (-->):** Gently tilt and rock back-and-forth horizontally $\rightarrow$ Displays **`-->`** (Silent).
- **Playing (*):** Vigorously shake the board in multiple axes $\rightarrow$ Displays **`*`** (Silent).
- **Anxious Pacing (!):** Rock at walking speed while flipping 180° back and forth $\rightarrow$ Displays **`!`** and **sounds 2 kHz acoustic chime**.
- **Alert Freeze ([]):** Hold vertically upright (head-high stance) motionless $\rightarrow$ Displays **`[]`** and **sounds 2 kHz acoustic chime**.

### Step 3: Run Real-Time Dashboards
- **Zero-Install Web Serial Dashboard:** Open [`pawstate_dashboard.html`](pawstate_dashboard.html) in Chrome/Edge and click **"Connect via USB"**.
- **Local Wi-Fi Mobile Bridge:** Run `python dashboard_bridge.py` and open the displayed URL on any smartphone connected to the same Wi-Fi.

---

## 4. System Architecture & μT-Kernel 3.0 Tasks

```
+---------------------------------------------------------------------------------------+
|                                μT-Kernel 3.0 Architecture                              |
+---------------------------------------------------------------------------------------+
        |
 [Hardware Timer 50Hz]
        | (tk_wup_tsk)
        v
 +-------------------------+     Lock-Free Circular Buffer
 | Task 1: IMU Sampler     | -------------------------------+
 | Priority: 1 (Highest)   |                                |
 +-------------------------+                                v
        | (tk_set_flg: EVT_NEW_SAMPLES)            +----------------------------+
        +----------------------------------------> | Task 2: Feature Extractor  |
                                                   | Priority: 5                |
                                                   +----------------------------+
                                                           | (tk_set_flg: EVT_FEATURES_READY)
                                                           v
                                                   +----------------------------+
                                                   | Task 3: TinyML Classifier  |
                                                   | Priority: 10               |
                                                   +----------------------------+
                                                           |            |
                                [Normal State Transition]  |            | [Anxiety Spike!]
                                (tk_snd_mbf)               |            | (tk_set_flg: EVT_ANXIETY_SPIKE)
                                                           v            v
                                                   +----------------------------+
                                                   | Task 4: BLE Event Logger   |
                                                   | Priority: 15 (Lowest)      |
                                                   +----------------------------+
```

### Task Specifications

| Task | Priority | Period / Context | Stack Size | Function & Design Decisions |
| :--- | :---: | :--- | :---: | :--- |
| **`tsk_imu_sampler`** | `1` (Highest) | 50 Hz (20 ms) | 512 B | Woken by cyclic handler `cyc_imu_sample`. Samples LSM303AGR accelerometer and magnetometer via 400kHz I2C (`sem_i2c`). Writes raw samples lock-free into a 256-entry circular buffer. Signals `EVT_NEW_SAMPLES` every 62 samples. |
| **`tsk_feature_extractor`** | `5` | ~1.24 s hop | 1024 B | Waits on `EVT_NEW_SAMPLES`. Peeks 125 samples (2.5s window), extracts 6-D fixed-point (Q16.16) features, consumes 62 samples, updates `current_features` under `sem_feature_buf`, and flags `EVT_FEATURES_READY`. |
| **`tsk_classifier`** | `10` | Event-driven | 1536 B | Waits on `EVT_FEATURES_READY`. Executes INT8 neural network forward pass, applies temporal debouncing, updates 5×5 LED matrix. If an anxiety spike occurs (Classes 3 & 4), immediately triggers the 2kHz buzzer and flags `EVT_ANXIETY_SPIKE`. |
| **`tsk_ble_logger`** | `15` | Event-driven | 1024 B | Receives state events via message buffer `mbf_ble_events`. Broadcasts BLE GATT notifications. In offline mode, caches up to 32 transitions in an onboard circular buffer and auto-replays upon reconnection. |

### Synchronization & Communication Primitives
- **`sem_i2c`:** Binary semaphore (`TA_TPRI`) guarding shared I2C bus transactions.
- **`sem_feature_buf`:** Mutex semaphore protecting the 6-D feature vector between extractor and classifier.
- **`flg_pipeline`:** Multi-wait event flag (`TA_WMUL`) coordinating pipeline execution stages.
- **`mbf_ble_events`:** FIFO message buffer (`TA_TFIFO`) queuing 12-byte `ble_event_t` packets.
- **`cyc_imu_sample`:** 50 Hz cyclic handler running in timer interrupt context to trigger jitter-free sampling.
- **`cyc_led_refresh`:** 120 Hz cyclic handler driving active row-scanning of the 5×5 LED matrix.

---

## 5. Canine Behavioral States & 5×5 LED Matrix Displays

| Class ID | Behavior | Biomechanical Profile | 5×5 LED Matrix Icon | Collar Alarm | Mobile Alert |
| :---: | :--- | :--- | :---: | :---: | :---: |
| **0** | **Resting** | Low variance ($\text{var} < 1800$), no zero-crossings, flat neck pitch ($0^\circ-12^\circ$). | `Z` (Sleep Symbol) | Silent | Normal Log |
| **1** | **Walking** | Rhythmic forward gait (~1.8 Hz), variance $3,500-17,500$, steady heading. | `-->` (Right Arrow) | Silent | Normal Log |
| **2** | **Playing** | Violent kinetic bursts ($\text{var} > 17,500$, g-load > 1.35g), jumping, romping. | `*` (Star / Spark) | Silent | Normal Log |
| **3** | **Anxious Pacing** | Repetitive walking + frequent 180° turns ($\Delta M > 50$ on compass). Stress indicator. | `!` (Exclamation) | **2kHz Chime** | **CRITICAL SPIKE** |
| **4** | **Alert Freeze** | Sudden tonic immobility ($\text{var} < 1800$), stiff upright neck pitch ($35^\circ-65^\circ$). Fear/threat. | `[]` (Rigid Box) | **2kHz Chime** | **CRITICAL SPIKE** |
| **0xFF**| **Unknown** | Model confidence $< 50\%$. Debouncer holds prior state to prevent flicker. | `?` (Question Mark)| Silent | Filtered |

### Hardware 5×5 LED Matrix Patterns
```
  Resting (Z)       Walking (-->)      Playing (*)     Anxious Pacing (!)   Alert Freeze ([])
  # # # # #         . . # . .          . . # . .           . . # . .            # # # # #
  . . . # .         . . . # .          . # # # .           . . # . .            # . . . #
  . . # . .         # # # # #          # # # # #           . . # . .            # . . . #
  . # . . .         . . . # .          . # # # .           . . . . .            # . . . #
  # # # # #         . . # . .          . # . # .           . . # . .            # # # # #
```

---

## 6. Verification & Sensitivity Suite

All components are accompanied by automated verification scripts to ensure 1:1 mathematical parity with the embedded C firmware:

### 6.1 Sensitivity & Edge-Case Benchmark (`test_sensitivity.py`)
```bash
python test_sensitivity.py
```
```
Case                             | Predicted       | Conf  | Probs (R,W,P,AP,AF) | Status
-----------------------------------------------------------------------------------------
Resting flat on table            | Resting         |  98%  | [251,   0,   0,   0,   0] | PASS
Resting slight breathing         | Resting         |  98%  | [251,   0,   0,   0,   0] | PASS
Slow walking / gentle stroll     | Walking         |  98%  | [  0, 251,   0,   0,   0] | PASS
Normal steady walking            | Walking         |  98%  | [  0, 251,   0,   0,   0] | PASS
Brisk walking / trot             | Walking         |  98%  | [  0, 251,   0,   0,   0] | PASS
Moderate shake in hand           | Walking         |  74%  | [  0, 190,  62,   0,   0] | PASS
Violent shaking in hand (Play)   | Playing         |  98%  | [  0,   0, 251,   0,   0] | PASS
Pacing with turns                | Anxious Pacing  |  98%  | [  0,   0,   0, 251,   0] | PASS (Alert Active)
Standing still upright (Freeze)  | Alert Freeze    |  98%  | [  0,   0,   0,   0, 251] | PASS (Alert Active)
Held vertically still in hand    | Alert Freeze    |  98%  | [  0,   0,   0,   0, 251] | PASS (Alert Active)
```

### 6.2 Independent Pipeline Verification (`verify_model_pipeline.py`)
```bash
python verify_model_pipeline.py
```
Validates the entire pipeline from raw IMU samples through fixed-point feature extraction, INT8 forward propagation, softmax normalization, anxiety alert triggering, and 5×5 LED matrix rendering (**100% PASS**).

---

## 7. Building from Source

### Prerequisites
- `arm-none-eabi-gcc` (Version 10.3 or higher with Cortex-M4 hard-float support)
- GNU Make (`make`)
- Python 3.9+ (with `numpy`, `tensorflow`, `pyserial`)

### Compilation Commands
```bash
# 1. Navigate to the μT-Kernel 3.0 build directory
cd mtk3/mtkernel_3/build_make

# 2. Compile the kernel and PawState application
make all

# 3. Generate Intel HEX binary for flashing
arm-none-eabi-objcopy -O ihex mtkernel_3.elf mtkernel_3.hex

# 4. Check memory consumption
arm-none-eabi-size mtkernel_3.elf
```

---

## 8. Repository Structure

```
c:\pawstate\
├── README.md                        # Master contest documentation (this file)
├── mtkernel_3.hex                   # Pre-compiled flashable binary for micro:bit v2
├── pawstate_dashboard.html          # Zero-install Web Serial & SSE live monitor
├── dashboard_bridge.py              # USB-to-HTTP/SSE Wi-Fi bridge for smartphone monitoring
├── test_sensitivity.py              # Physical & hand demonstration benchmark script
├── verify_model_pipeline.py         # Independent pipeline verification agent
├── train_pawstate.py                # Continuum training & INT8 C header exporter
├── docs/                            # Official Contest Submission Documentation
│   ├── OPERATION_MANUAL.md          # Comprehensive user & operating manual
│   ├── PROCEDURE_MANUAL.md          # Step-by-step evaluation procedure for judges
│   └── SYSTEM_ARCHITECTURE_AND_DESIGN.md # In-depth technical specification
├── mtk3/mtkernel_3/                 # Active μT-Kernel 3.0 RTOS Source Tree
│   ├── build_make/                  # Makefile and build output artifacts
│   ├── kernel/                      # μT-Kernel 3.0 core OS (scheduler, sync, memory)
│   ├── sysdepend/microbit/          # nRF52833 hardware initialization & vectors
│   └── sample-pawstate/             # PawState Application Code (Active Build Root)
│       ├── app/                     # The 4 RTOS Tasks & Main Entry Point
│       ├── drivers/                 # Hardware Drivers (LSM303AGR, LED Matrix, PWM Buzzer, BLE)
│       ├── ml/                      # INT8 Inference Engine & model_data.h
│       ├── util/                    # Lock-free circular buffer & Q16.16 math utils
│       └── include/                 # Configuration headers & type definitions
└── sample-pawstate/                 # Synchronized mirror directory
```

---

## 9. TRON Contest Compliance Summary

1. **Native μT-Kernel 3.0 Conformance:** Fully leverages native μT-Kernel tasks, semaphores, event flags, message buffers, and cyclic handlers without external RTOS abstractions.
2. **Deterministic Hard Real-Time Execution:** Priority-preemptive scheduling guarantees 50 Hz IMU sensor integrity while running machine learning and communication tasks asynchronously.
3. **Pure C Edge AI:** Demonstrates how INT8 TinyML inference can run in micro-seconds on a Cortex-M4 microcontroller without bloated runtimes.
4. **Accessible Real-World Impact:** Turns an affordable, ubiquitous educational board (BBC micro:bit v2) into a sophisticated veterinary wellness wearable.

---
*PawState — TRON Programming Contest 2026 Submission*
