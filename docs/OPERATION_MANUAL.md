# PawState: Real-Time Canine Welfare & Anxiety Monitor
## OPERATION MANUAL
**Document Version:** 1.0.0  
**Target Platform:** BBC micro:bit v2 (Nordic nRF52833 ARM Cortex-M4F)  
**Operating System:** μT-Kernel 3.0 Real-Time Operating System  
**Submission Category:** TRON Programming Contest 2026  

---

## 1. Introduction & Device Overview

**PawState** is an intelligent, collar-mounted wearable device engineered to monitor canine emotional health, physical activities, and sudden acute anxiety episodes in real time. Operating entirely at the edge without cloud dependency, PawState executes high-frequency inertial sensing, 6-dimensional biomechanical feature extraction, and INT8 quantized neural network inference directly on the **BBC micro:bit v2** powered by **μT-Kernel 3.0**.

### 1.1 Hardware Specifications
- **Microcontroller:** Nordic Semiconductor nRF52833 (ARM Cortex-M4F with hardware FPU @ 64 MHz)
- **Memory:** 512 KB on-chip Flash, 128 KB SRAM
- **Inertial Measurement Unit (IMU):** STMicroelectronics LSM303AGR (3-axis accelerometer $\pm 2g$, 3-axis magnetometer) on internal I2C bus (`0x19`, `0x1E`)
- **Visual Display:** 5×5 Red LED Matrix (120 Hz row-multiplexed hardware scanner)
- **Acoustic Indicator:** On-board electromagnetic buzzer driven by hardware PWM (`P0.00`)
- **Wireless Radio:** Bluetooth 5.0 Low Energy (2.4 GHz) with custom GATT service
- **Physical Interface:** USB Type-B micro connector (UART telemetry & flashing), JST-PH 2-pin battery connector (3.0 V)

---

## 2. Collar Mounting & Physical Orientation

To guarantee accurate biomechanical classification, PawState must be oriented correctly on the dog's collar:

```
                  Top of Collar (Skyward)
                      +--------------+
                      |   [  USB  ]  |
                      |              |
     Left Shoulder <--|   5x5 LED    |--> Right Shoulder
        (-Y Axis)     |    Matrix    |      (+Y Axis)
                      |              |
                      |   [LOGO]     |
                      +--------------+
                 Bottom of Collar (Groundward)
                            (+X Axis)
                   Z-Axis: Pointing Away from Dog Neck
```

### Mounting Guidelines:
1. **Positioning:** Mount the micro:bit vertically against the front or side of the dog's collar using a silicone enclosure or velcro collar strap.
2. **Axis Alignment:**
   - **X-Axis:** Points downwards toward the dog's chest/ground.
   - **Y-Axis:** Aligns laterally across the dog's shoulders (left-to-right).
   - **Z-Axis:** Points outward away from the dog's neck.
3. **Collar Snugness:** The collar should fit comfortably snug (allowing two fingers between collar and neck) so that vibrations and head movements are transmitted faithfully without bouncing against the fur.

---

## 3. Power Modes

PawState supports two primary operating power modes:

### 3.1 Standalone Battery Operation (Daily Canine Wear)
- **Power Source:** Connect an external 2×AAA battery pack (3.0 V) or a single-cell LiFePO4 battery to the micro:bit's 2-pin JST-PH connector.
- **Boot Sequence:**
  1. Once power is connected, the micro:bit yellow power LED illuminates.
  2. μT-Kernel 3.0 initializes the hardware peripherals, registers 4 RTOS tasks, and begins 120 Hz LED matrix scanning.
  3. The display illuminates the initial state (Resting `Z` or current movement).
- **Independence:** In this mode, PawState functions 100% autonomously. Behavioral classifications appear on the 5×5 LED display, anxiety chimes sound from the on-board buzzer, and telemetry is broadcast over Bluetooth Low Energy (BLE).

### 3.2 USB Demonstration & Telemetry Mode
- **Power & Data Source:** Connect the micro:bit via micro-USB to a PC, laptop, or USB power bank.
- **Serial Output:** The device streams real-time diagnostic telemetry over USB CDC UART at **115200 baud, 8-N-1**.
- **Interactive Dashboards:** Enables connection to the PC Web Serial dashboard or the Wi-Fi mobile bridge server.

---

## 4. Visual Display Guide (5×5 LED Matrix)

The on-collar 5×5 LED matrix provides instant visual status of the canine's behavioral state. Driven by a 120 Hz hardware row-multiplexed cyclic handler in μT-Kernel 3.0, the display is bright, stable, and flicker-free.

| State | 5×5 LED Pattern | Icon Description | Canine Behavior & Biomechanics |
| :--- | :---: | :--- | :--- |
| **Resting** | <pre># # # # #<br>. . . # .<br>. . # . .<br>. # . . .<br># # # # #</pre> | **`Z` (Sleep Symbol)** | Inactive, lying down, sitting still, sleeping. Low motion variance ($\text{var} < 1800$), flat neck pitch ($0^\circ - 12^\circ$). |
| **Walking** | <pre>. . # . .<br>. . . # .<br># # # # #<br>. . . # .<br>. . # . .</pre> | **`-->` (Right Arrow)** | Forward steady locomotion, rhythmic cadence (~1.8 Hz), regular gait. Variance $3,500 - 17,500$. |
| **Playing** | <pre>. . # . .<br>. # # # .<br># # # # #<br>. # # # .<br>. # . # .</pre> | **`*` (Star / Spark)** | Energetic play, romping, jumping, running bursts, body shakes. High acceleration load ($\text{var} > 17,500$, g-force $> 1.35g$). |
| **Anxious Pacing** | <pre>. . # . .<br>. . # . .<br>. . # . .<br>. . . . .<br>. . # . .</pre> | **`!` (Exclamation)** | **STRESS SPIKE:** Repetitive pacing back and forth with frequent 180° turns near doors/fences. Detected via high magnetometer flux ($\Delta M > 50$). |
| **Alert Freeze** | <pre># # # # #<br># . . . #<br># . . . #<br># . . . #<br># # # # #</pre> | **`[]` (Rigid Box)** | **FEAR / THREAT SPIKE:** Sudden complete postural stiffness (tonic immobility) with head raised high ($\text{pitch} \ge 35^\circ$). Detected via zero motion + high posture angle. |
| **Unknown** | <pre>. # # # .<br>. . . # .<br>. . # . .<br>. . . . .<br>. . # . .</pre> | **`?` (Question Mark)** | Ambiguous motion or rapid transient state shift with model confidence $< 50\%$. The debouncer holds prior states to prevent brief flickers. |

---

## 5. Acoustic Warning Alarm System

When an acute emotional distress episode is identified, PawState triggers an immediate local audio alert to alert the dog owner or handler in the room without requiring a smartphone:

1. **Trigger Conditions:**
   - **Anxious Pacing** (Class 3) with model confidence $\ge 55\%$.
   - **Alert Freeze** (Class 4) with model confidence $\ge 55\%$.
2. **Acoustic Characteristics:**
   - **Frequency:** 2,000 Hz (resonant peak of micro:bit v2 magnetic buzzer).
   - **Pattern:** Triple-burst chime (150 ms tone, 100 ms pause, 150 ms tone, 100 ms pause, 150 ms tone).
   - **Preemption Latency:** Executed within **<30 ms** of feature computation via kernel event flag `EVT_ANXIETY_SPIKE`.
3. **Quiet / Standby Conditions:**
   - The buzzer is completely silent during normal Resting, Walking, and Playing behaviors to conserve battery and avoid annoying the pet.

---

## 6. Remote Monitoring & Live Dashboards

PawState provides three complementary ways to monitor collar status remotely:

### 6.1 Direct Bluetooth Low Energy (BLE) Mobile Connection
- **Protocol:** Bluetooth 5.0 GATT.
- **Broadcast Name:** `PawState`
- **Supported Apps:** nRF Connect (iOS/Android), LightBlue, Web Bluetooth browsers.
- **Custom GATT Welfare Service UUID:** `0000180D-0000-1000-8000-00805F9B34FB` (Canine Health Profile)
  - **Behavior State Characteristic (`UUID: 0x2A37`):** Notifies 1-byte state ID (`0`=Resting, `1`=Walking, `2`=Playing, `3`=Pacing, `4`=Freeze) + 1-byte confidence percentage ($0–100\%$) + 4-byte uptime timestamp.
  - **Anxiety Alert Characteristic (`UUID: 0x2A3F`):** High-priority notification sent immediately when an anxiety spike occurs, triggering phone vibration and banner alerts.

### 6.2 Local Wi-Fi Mobile Dashboard (`dashboard_bridge.py`)
For homes, clinics, or lab demonstrations where the micro:bit is connected via USB:
1. Ensure your PC and smartphone are connected to the same home Wi-Fi network.
2. Run the bridge script on your PC:
   ```powershell
   python dashboard_bridge.py --port COMx --baud 115200
   ```
3. The terminal displays your local network address:
   ```
   ======================================================================
   🐾 PawState Real-Time Canine Monitor — Wi-Fi & Web Bridge
   ======================================================================
     Local PC Dashboard:     http://localhost:8080
     Mobile Phone Dashboard: http://192.168.1.XX:8080
   ======================================================================
   ```
4. Open the `http://192.168.1.XX:8080` URL in Safari or Chrome on your mobile phone.
5. The mobile screen mirrors the collar's 5×5 LED matrix in real time, displays live confidence graphs, and plays audio alert chimes upon anxiety events!

### 6.3 Zero-Install Web Serial Dashboard (`pawstate_dashboard.html`)
1. Open [`pawstate_dashboard.html`](../pawstate_dashboard.html) directly in Google Chrome or Microsoft Edge.
2. Click **"🔌 Connect via USB (Web Serial)"** and choose the `BBC micro:bit CMSIS-DAP` COM port.
3. No Python or web server required — telemetry is parsed directly in browser JavaScript.

---

## 7. Offline Data Logging & Historical Sync

When the dog is outside Bluetooth range (e.g., roaming in the yard):
1. **Automatic Detection:** The BLE logger task (`tsk_ble_logger`) monitors connection status.
2. **Ring Buffer Storage:** Behavioral state changes and timestamped anxiety events are automatically saved into an onboard 2880-entry circular ring buffer, sized for approximately 24 hours at two state transitions per minute.
3. **Reconnection Burst Sync:** As soon as the dog returns within Bluetooth range, PawState automatically flushes the stored event log to the paired smartphone, ensuring no anxiety spikes are lost.

---

## 8. Battery Life & Power Management

| Operating Condition | Estimated Current Consumption | Battery Life (2x AAA Alkaline, 1000 mAh) |
| :--- | :---: | :---: |
| **Continuous Active Monitoring** (50 Hz IMU + 120 Hz LED + 1.24s TinyML) | ~18 mA | **~55 Hours** (~2.3 Days) |
| **Resting Low-Power Mode** (IMU 10 Hz + LED Dimmed) | ~4.2 mA | **~238 Hours** (~10 Days) |
| **Standby / Sleep** | < 0.8 mA | **> 50 Days** |

---

## 9. Troubleshooting & FAQ

| Symptom | Probable Cause | Corrective Action |
| :--- | :--- | :--- |
| **LED matrix remains blank on boot** | Power not connected or corrupt flash. | Check battery voltage (>2.7V). Re-drag [`mtkernel_3.hex`](../mtkernel_3.hex) onto `MICROBIT` drive. |
| **Device displays `?` (Unknown) frequently** | Rapid, erratic hand manipulation or loose collar. | Ensure collar is comfortably snug. Allow 1-2 seconds of steady motion for the 1.24s sliding window to capture cadence. |
| **No sound during Anxiety Spikes** | Volume disabled or board rev mismatch. | Ensure micro:bit v2 is used (v1 does not have built-in speaker). Check that `P0.00` PWM is enabled. |
| **Web Serial dashboard cannot connect** | COM port occupied by another terminal. | Close PuTTY, Tera Term, or Arduino Serial Monitor before clicking "Connect" in the dashboard. |
| **Mobile Wi-Fi bridge unreachable** | PC firewall blocking port 8080 or phone on cellular data. | Ensure phone is on the same local Wi-Fi SSID as the PC. Allow port 8080 through Windows Defender Firewall. |

---
*PawState — Engineered for Canine Welfare on μT-Kernel 3.0 & BBC micro:bit v2*
