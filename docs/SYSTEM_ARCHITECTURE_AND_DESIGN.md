# PawState: Real-Time Canine Welfare & Anxiety Monitor
## SYSTEM ARCHITECTURE & TECHNICAL DESIGN SPECIFICATION
**Document Version:** 1.0.0  
**Target Platform:** BBC micro:bit v2 (Nordic nRF52833 ARM Cortex-M4F)  
**Operating System:** μT-Kernel 3.0 Real-Time Operating System  
**Submission Category:** TRON Programming Contest 2026  

---

## 1. Architectural Philosophy & Design Principles

PawState demonstrates how a hard real-time operating system (**μT-Kernel 3.0**) combined with deterministic **TinyML edge intelligence** can transform an accessible, low-cost microcontroller (BBC micro:bit v2) into a mission-critical veterinary wearable.

### Core Engineering Principles:
1. **Zero Dynamic Memory Allocation:** Zero calls to `malloc()` or dynamic heaps. All task stacks, circular buffers, kernel control blocks, and neural network weights are statically allocated at link time, preventing heap fragmentation and memory leaks.
2. **Strict Priority Preemption:** High-frequency 50 Hz IMU sensor acquisition runs at Priority 1, completely immune to preemption by machine learning inference or communication tasks.
3. **Pure C Embedded TinyML:** Rather than incorporating heavyweight C++ runtimes like TensorFlow Lite for Microcontrollers (~80 KB flash overhead), PawState uses a custom, highly optimized pure C INT8 feedforward engine (~500 bytes code, 290 bytes weights) that executes deterministically in < 2.5 ms.
4. **Sub-1.3s Responsive Latency:** A 50% overlapping sliding window evaluates canine behavior every 1.24 seconds without sacrificing the 2.5-second observation context required for low-frequency stride cadence.

---

## 2. μT-Kernel 3.0 Task Pipeline & Concurrency Model

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

### 2.1 Task Specifications

| Task Identifier | Task Function | Priority | Execution Context | Stack Size | Description |
| :--- | :--- | :---: | :--- | :---: | :--- |
| **`tsk_imu_sampler`** | `imu_sampler_task()` | `1` | Woken by cyclic handler every 20 ms | 512 B | Acquires 6-axis data (Accel X,Y,Z + Mag X,Y,Z) from LSM303AGR via 400kHz I2C (`sem_i2c`). Pushes samples into a 256-entry lock-free circular buffer. Signals `EVT_NEW_SAMPLES` every 62 samples (1.24s). |
| **`tsk_feature_extractor`** | `feature_extractor_task()` | `5` | Event-driven (`EVT_NEW_SAMPLES`) | 1024 B | Peeks 125 samples (2.5s window) from buffer, computes 6-D fixed-point (Q16.16) features, consumes 62 samples, updates `current_features` under `sem_feature_buf`, and signals `EVT_FEATURES_READY`. |
| **`tsk_classifier`** | `tinyml_classifier_task()` | `10` | Event-driven (`EVT_FEATURES_READY`) | 1536 B | Copies features, runs INT8 neural network inference, applies temporal debouncing, updates 5×5 LED matrix. If an anxiety spike is detected (Classes 3 & 4), immediately sounds the 2kHz buzzer and signals `EVT_ANXIETY_SPIKE`. |
| **`tsk_ble_logger`** | `ble_logger_task()` | `15` | Event-driven (`mbf_ble_events` / `EVT_ANXIETY_SPIKE`) | 1024 B | Dequeues state events and broadcasts BLE GATT notifications. In offline mode, caches events in an onboard 32-entry circular buffer. |

### 2.2 Cyclic Handlers & Interrupt Service Contexts

- **`cyc_imu_sample` (50 Hz / 20 ms period):** Registered via `tk_cre_cyc()`. Executes in high-resolution timer interrupt context. Wakes `tsk_imu_sampler` with zero software jitter.
- **`cyc_led_refresh` (120 Hz / 8.33 ms period):** Executes row-multiplexed scanning of the 5×5 LED matrix. Scans rows 1 through 5 sequentially with active-high anode rows and active-low cathode columns, ensuring flicker-free persistence of vision without CPU-intensive software loops.

### 2.3 Inter-Process Communication (IPC) Primitives

1. **`sem_i2c` (`TA_TPRI`):** Binary mutex semaphore protecting the shared internal I2C bus between accelerometer and magnetometer register reads.
2. **`sem_feature_buf` (`TA_TPRI`):** Mutex semaphore ensuring atomic access to the 6-D fixed-point feature vector between `tsk_feature_extractor` and `tsk_classifier`.
3. **`flg_pipeline` (`TA_WMUL`):** Multi-wait event flag group coordinating data flow:
   - `0x0001` (`EVT_NEW_SAMPLES`): Buffer contains a new 62-sample step.
   - `0x0002` (`EVT_FEATURES_READY`): Features extracted and ready for ML inference.
   - `0x0004` (`EVT_ANXIETY_SPIKE`): High-priority urgent anxiety preemption flag.
4. **`mbf_ble_events` (`TA_TFIFO`):** FIFO message buffer allowing asynchronous transmission of 12-byte `ble_event_t` packets from classifier to BLE logger.

---

## 3. 6-Dimensional Biomechanical Feature Extraction Engine

The feature extractor transforms raw, noisy 50 Hz IMU streams into 6 invariant canine biomechanical descriptors using deterministic **Q16.16 fixed-point arithmetic**:

### 3.1 Feature Definitions

1. **Combined Acceleration Variance ($f_1$):**
   $$\sigma_{\text{tot}} = \sigma_x + \sigma_y + \sigma_z$$
   Measures total movement kinetic energy. Distinguishes stillness ($\sigma < 1800$) from steady locomotion ($3500 - 17500$) and violent play ($> 17500$).

2. **Mean Acceleration Magnitude ($f_2$):**
   $$f_2 = \frac{1}{N} \sum_{i=1}^{N} \sqrt{a_{x,i}^2 + a_{y,i}^2 + a_{z,i}^2}$$
   Computed with 64-bit integer accumulation to prevent 32-bit overflow at $2g$ full-scale. Quantifies effective gravitational load.

3. **Magnetometer RMS Delta ($f_3$):**
   $$\Delta M_{\text{RMS}} = \sqrt{\frac{1}{N-1} \sum_{i=1}^{N-1} \left( (\Delta m_x)^2 + (\Delta m_y)^2 + (\Delta m_z)^2 \right)}$$
   Acts as a gyroscope proxy on the micro:bit v2. Detects the repetitive 180° directional reversals that characterize canine anxious pacing.

4. **Posture & Tilt Angle ($f_4$):**
   $$\theta = \arctan\left(\frac{a_x}{\sqrt{a_y^2 + a_z^2}}\right)$$
   During dynamic motion, calculates range $\Delta \theta = \theta_{\max} - \theta_{\min}$. When motionless ($\Delta \theta < 15^\circ$), switches to absolute neck pitch to distinguish Alert Freeze (head held upright at $35^\circ - 65^\circ$) from Resting (head flat at $0^\circ - 12^\circ$).

5. **Zero-Crossing Cadence Rate ($f_5$):**
   Counts X-axis acceleration zero crossings with a $\pm 1000$ mg hysteresis band, extracting canine stride cadence (~1.8 Hz).

6. **Activity Bout Duration ($f_6$):**
   Measures cumulative time (ms) where acceleration deviation from $1g$ exceeds $\pm 120$ mg, capturing sustained locomotion endurance.

---

## 4. 50% Overlapping Sliding Window Mechanics

```
Sample Stream (50 Hz = 20ms/sample):
[-- Old Window (125 samples = 2.5s) --]
                     [-- Step Size (62 samples = 1.24s) --]
                     [-- New Window (125 samples = 2.5s) --]
```

- **Ring Buffer Size:** 256 samples (`imu_sample_t` = 12 bytes/sample = 3,072 bytes).
- **Buffer Mechanism:** Power-of-2 circular buffer using bitwise mask (`head & 0xFF`).
- **Peeking vs Consuming:**
  - Feature extractor peeks 125 contiguous samples via `buf_peek_window()`.
  - Feature extractor consumes only 62 samples via `buf_consume(FEATURE_STEP_SIZE)`.
- **Latency Advantage:** New predictions are generated every **1.24 seconds** rather than 2.5 seconds, cutting user-perceived state transition lag by **50%**.

---

## 5. INT8 Quantized Neural Network & Embedded Inference Engine

### 5.1 Model Topology
- **Input Dimension:** 6 features $\rightarrow$ Quantized to `int8_t` $[-128, 127]$.
- **Hidden Layer 1:** 16 neurons with ReLU activation, INT8 weights, INT32 accumulator, shifted requantization (`acc >> 8`).
- **Hidden Layer 2:** 8 neurons with ReLU activation, INT8 weights, INT32 accumulator (`acc >> 8`).
- **Output Layer:** 5 neurons (logits).
- **Softmax Engine:** Fast piecewise-linear fixed-point integer normalization converting logits into normalized probabilities $[0, 255]$:
  $$\text{score}_i = \max(1, 256 + 4 \times (\text{logit}_i - \text{logit}_{\max}))$$
  $$P_i = \frac{\text{score}_i \times 255}{\sum \text{score}}$$

### 5.2 Temporal Persistence Filter & Debouncer

To eliminate question-mark flickering and transient noise without compromising emergency alerting:
- **Instant Anxiety Escalation:** High-priority anxiety spikes (Classes 3 & 4) trigger instantly with zero delay (<30ms).
- **Instant Confident Switching:** Any classification with $\ge 70\%$ confidence transitions immediately.
- **Moderate Confirmation (50–69%):** Requires 2 consecutive matching sliding windows.
- **Graceful State Holding (<50%):** A single ambiguous window holds the previous known state rather than dropping to `Unknown` (`?`).

---

## 6. Hardware Peripheral Subsystems

### 6.1 LSM303AGR 6-Axis IMU (I2C Driver)
- **Interface:** Hardware TWIM0 peripheral (`P0.08` SCL, `P0.16` SDA) running at 400 kHz Fast-Mode.
- **Accelerometer:** 50 Hz Normal Mode, $\pm 2g$ full-scale, 10-bit resolution ($3.9 \text{ mg/LSB}$).
- **Magnetometer:** 50 Hz High-Resolution Mode, $\pm 50 \text{ Gauss}$.

### 6.2 5×5 LED Matrix (Display Driver)
- **Scanning Mode:** 120 Hz row-multiplexed active scanning driven by `cyc_led_refresh`.
- **Anode Rows (Active High):** `P0.21`, `P0.22`, `P0.15`, `P0.24`, `P0.19`.
- **Cathode Columns (Active Low):** `P0.28`, `P0.11`, `P0.31`, `P1.05`, `P0.30`.

### 6.3 Acoustic Buzzer (Audio Driver)
- **Pin:** `P0.00` connected to built-in magnetic speaker.
- **Controller:** Nordic PWM0 peripheral with hardware shortcut `PWM0_SHORTS = (1UL << 3)` (`LOOPSDONE_SEQSTART0`) and `PWM0_LOOP = 0xFFFF`.
- **Frequency:** 2,000 Hz tone generated entirely in hardware without CPU cycle consumption.

---

## 7. Memory Footprint & Resource Breakdown

### 7.1 Flash Memory Allocation (512 KB Total)
- **μT-Kernel 3.0 Core & HAL:** ~32 KB
- **PawState Application & Drivers:** ~14 KB
- **INT8 Neural Network Weights & Inference Engine:** ~1.8 KB
- **Remaining Flash Headroom:** **> 464 KB (90.6% Free)**

### 7.2 SRAM Allocation (128 KB Total)
- **Kernel Data Structures & System Stacks:** ~4.2 KB
- **Application Task Stacks (4 Tasks):** ~4.0 KB
- **IMU Circular Ring Buffer (256 samples):** ~3.1 KB
- **BLE Offline Cache & Static Buffers:** ~2.5 KB
- **Remaining SRAM Headroom:** **> 114 KB (89.0% Free)**

---
*PawState — Engineered for Canine Welfare on μT-Kernel 3.0 & BBC micro:bit v2*
