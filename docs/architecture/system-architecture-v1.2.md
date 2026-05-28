# Music Kendama — System Architecture v1.2 (Locked)

*Finalized architecture decisions from the Part 1 Phased Architectural Audit.*
*Status: **LOCKED** — All decisions below are committed unless testing reveals a specific, documented failure.*

**Changelog from v1.1:**

- Added debug broadcast port (localhost:9500) to compiled bridge spec — zero-cost passive diagnostic tap for any external visualization tool. Not a system dependency.
- Clarified that PWM refresh rate (20 kHz, hardware) is fully decoupled from software data frame rate. LED flicker is physically impossible regardless of software update rate. Animation stepping (POV spatial aliasing) is the actual concern at low frame rates, addressed by Layer 0 local rendering at 500 Hz.

**Changelog from v1.0:**

- Removed TouchDesigner from the architecture entirely — all LED control handled by firmware and Max for Live.
- Replaced Python bridge with compiled C bridge (primary) or direct M4L serial read; Python demoted to dev/debug tool.
- Specified dual USB-CDC serial ports on receiver (upstream sensor data, downstream LED commands) to eliminate bidirectional traffic contention.
- Added complete power budget with firmware current capping.
- Replaced fixed 60 fps LED assumption with priority-layered LED control system (local firmware at 500 Hz, M4L at 100–200 Hz).
- Added receiver-side coordinated LED computation (both device states visible to receiver, no laptop round-trip needed for coordinated effects).
- Added active-mode current comparison between ESP32-S3 and ESP32-C5 to support chip selection rationale.

---

## 1. Architecture Summary

A stringless jumbo kendama (25cm ken, 93mm tama) with embedded IMU sensors, addressable LEDs, and wireless OSC communication for real-time musical performance control in Ableton Live via Max for Live. The system uses ESP-NOW for all wireless communication, OSC as the sole data protocol at every layer, and a hardwired USB receiver bridging to the laptop.

**No MIDI anywhere. No TouchDesigner. No dedicated WiFi router.**

**Performance software stack: Ableton Live. That's it.** Everything else is firmware.

---

## 2. Network Topology: Parallel Clients (Approach A)

The Tama and Ken operate as completely independent, isolated wireless nodes. Each streams its own sensor data directly to a shared USB receiver. The receiver also serves as the coordination point for LED effects that require awareness of both devices simultaneously. Neither instrument depends on the other for transmission timing or data availability.

**Why Approach A over Approach B (Serial Daisy-Chain):**

- Zero serialization dependency — a delayed Tama packet has no impact on Ken telemetry, and vice versa.
- Single radio state transition per cycle (Tx only) versus four transitions in a daisy-chain (Rx→Tx→Rx→Tx), eliminating 200–800 µs of dead air per cycle from half-duplex turnaround.
- Full failure isolation — a dropped packet from one device only affects that device's data. In a daisy-chain, a single dropped consolidated packet loses both datasets simultaneously.
- No two-hop radio latency penalty (1.5–3.0 ms saved).
- All mapping and processing logic lives in software (Max for Live / receiver firmware), where iteration is fast.

---

## 3. Communication Protocol: OSC Everywhere

**OSC (Open Sound Control) is the sole data protocol at every layer of the system.** The physical transports change (ESP-NOW radio, USB serial, UDP socket), but the data format and addressing structure is OSC from sensor to speaker.

**Why not MIDI:**

- 7-bit value resolution (0–127) is inadequate for continuous sensor data; OSC uses 32-bit floats.
- MIDI's 16-channel model doesn't map to hierarchical sensor namespaces; OSC addresses are self-documenting strings (`/kendama/tama/orientation/roll`).
- MIDI CC numbers are opaque; OSC paths are human-readable months later.
- No arbitrary message count limitation.
- No protocol translation anywhere in the chain.

**Protocol at each hop:**

| Hop | Physical Transport | Data Format |
|-----|--------------------|-------------|
| IMU → ESP32 | I2C (internal) | Quaternion + raw accel (sensor registers) |
| ESP32 → Receiver | ESP-NOW (2.4 GHz, connectionless) | OSC-structured binary payload |
| Receiver → Laptop | USB-Serial Port A (SLIP-framed) | SLIP-encoded OSC packets |
| Bridge → M4L | UDP localhost:9000 | Standard OSC/UDP |
| M4L → Ableton | Live API (`live.remote~`) | Native parameter control |
| M4L → Receiver | UDP localhost:9001 → USB-Serial Port B | SLIP-encoded OSC LED commands |
| Receiver → Instruments | ESP-NOW (2.4 GHz) | OSC-structured LED frames |

---

## 4. Wireless Layer: ESP-NOW (No WiFi Infrastructure)

All wireless communication uses ESP-NOW at 2.4 GHz. No WiFi association, no DHCP, no router, no access point, no infrastructure mode.

**Why ESP-NOW over WiFi:**

- Connectionless — no association handshake, no reconnection delays if signal drops momentarily.
- ~0.3 ms single-hop latency for a 250-byte frame (versus 2–5 ms for WiFi UDP with CSMA/CA contention).
- No router to power, carry, configure, or debug on stage.
- Peer-to-peer with registered MAC addresses — deterministic, no channel contention from other devices.
- 99.5%+ packet delivery rate indoors at <10 m with default Tx power; effectively perfect at 20 dBm with line-of-sight on stage.
- ESP-NOW v2.0 supports up to 1470-byte payloads (more than sufficient).

**ESP-NOW Configuration:**

- Channel: Fixed (default channel 1, or configurable to avoid local interference).
- Tx Power: 20 dBm (maximum, for stage reliability).
- Peers: Receiver registers two peers (Tama MAC, Ken MAC); each instrument registers one peer (Receiver MAC).
- Frame type: Unicast (ACK-confirmed delivery).
- Encryption: Optional (LMK per-peer if needed for public performances).

---

## 5. Receiver: Hardwired USB-Serial Bridge with Dual Ports

A dedicated ESP32-S3 plugged into the MacBook Pro via USB-C. Permanently wired — no wireless last mile.

The receiver presents as a **USB-CDC composite device with two virtual serial ports** using TinyUSB:

- **Port A (Upstream — Sensor Telemetry):** Instrument sensor data flows from ESP-NOW Rx → SLIP encode → Port A → laptop. Read-only from the laptop's perspective. This port is never blocked by downstream LED traffic.
- **Port B (Downstream — LED Commands):** LED commands flow from laptop → Port B → SLIP decode → ESP-NOW Tx to target device. Write-only from the laptop's perspective. This port is never blocked by upstream sensor reads.

**Why dual ports:** Upstream and downstream share a single USB connection but are logically independent. A burst of LED frames from M4L writing to Port B cannot delay or block the bridge reading sensor data from Port A. The OS presents them as two independent `/dev/tty.*` devices.

### Receiver Firmware Responsibilities

1. Register two ESP-NOW peers (Tama, Ken).
2. On ESP-NOW Rx from instruments: SLIP-encode the OSC payload, write to USB-Serial **Port A**.
3. On USB-Serial **Port B** Rx from laptop: parse SLIP frames, route to target device via ESP-NOW.
4. **Coordinated LED computation (Layer 0.5):** When enabled, use both devices' latest sensor state to generate coordinated LED frames locally and transmit via ESP-NOW — no laptop round-trip. See Section 9 (LED Architecture) for details.

### Laptop-Side Bridge

**Primary (production/performance):** A compiled C bridge using `libserialport` + POSIX UDP sockets. ~80 lines. Deterministic, zero-allocation byte I/O. No GC pauses, no GIL, no runtime. Reads SLIP from Port A, sends UDP to localhost:9000. Reads UDP from localhost:9001, writes SLIP to Port B.

**Debug broadcast:** The bridge also sends a copy of all Port A data to localhost:9500 (UDP, fire-and-forget). This is a zero-cost debug affordance — UDP is connectionless, so if nothing is listening on 9500, packets hit the floor with no blocking, no handshake, and no latency impact on the primary 9000 path. Any visualization tool (TouchDesigner, Processing, a custom Python plotter, etc.) can bind to 9500 and observe the live sensor stream without touching the production signal chain. This port is not part of the system architecture — it's a passive diagnostic tap.

**Alternative (if M4L serial works well):** Direct M4L `[serial]` object reading Port A and writing Port B. Eliminates the bridge process entirely. Depends on M4L's serial polling rate being fast enough — needs benchmarking.

**Development/debugging only:** Python script using `pyserial` + `socket`. Convenient for printf-debugging and packet inspection. Not used in performance.

**Bridge-to-Ableton Latency (Port A path):** ~0.5–1.0 ms (USB-Serial read + UDP loopback). The debug broadcast to 9500 is non-blocking and adds zero latency to this path.

---

## 6. Hardware Bill of Materials (Locked Allocations)

### Microcontrollers: ESP32-S3 × 3

| Unit | Board | Role |
|------|-------|------|
| Tama | Adafruit ESP32-S3 Feather (ID: 5323) | IMU read, ESP-NOW Tx/Rx, LED control |
| Ken | Adafruit ESP32-S3 Feather (ID: 5323) | IMU read, ESP-NOW Tx/Rx, LED control |
| Receiver | Adafruit ESP32-S3 Feather (ID: 5323) | ESP-NOW Rx/Tx, dual USB-CDC serial, coordinated LED computation |
| Spare | Adafruit ESP32-S3 Feather (ID: 5323) | Reserve |

**Why ESP32-S3 over ESP32-C5:**

- **Dual symmetric cores** (2 × Xtensa LX7 @ 240 MHz) — pin WiFi/ESP-NOW stack to Core 0 and sensor/LED loop to Core 1 with `xTaskCreatePinnedToCore()`. Deterministic, jitter-free IMU sampling at 1 kHz regardless of radio activity. The C5's single HP core (240 MHz RISC-V) shares all tasks on one core via FreeRTOS scheduling — an ESP-NOW Rx callback can preempt a sensor read mid-computation.
- **Native USB OTG** with TinyUSB support — can present as dual USB-CDC composite device natively. The C5 only has USB Serial/JTAG controller (cannot present as arbitrary USB device classes, cannot do composite devices).
- **Mature ecosystem** — years of stable Arduino/ESP-IDF support, extensive community examples, proven ESP-NOW and TinyUSB implementations. The C5 reached stable ESP-IDF support in v6.0 (March 2026).
- **Active-mode power draw is equivalent:** Both chips draw 50–80 mA with WiFi/ESP-NOW radio active at 2.4 GHz. The RISC-V efficiency advantage applies to sleep modes (C5 LP core at 0.25 mA), not active radio operation. The kendama is always active during performance — sleep-mode efficiency is irrelevant.
- **C5's 5 GHz radio is unused** — ESP-NOW operates at 2.4 GHz. Wi-Fi 6 features (OFDMA, TWT) don't reduce single-packet latency on a two-device network.

### IMUs: LSM6DSV16X × 2 (One Per Unit)

| Unit | Sensor | Board | Role |
|------|--------|-------|------|
| Tama | LSM6DSV16X | SparkFun Micro 6DoF (SEN-21336) | Primary and only IMU |
| Ken | LSM6DSV16X | SparkFun Micro 6DoF (SEN-21336) | Primary and only IMU |

**Why LSM6DSV16X as sole IMU (no ISM330DHCX secondary):**

The LSM6DSV16X is not just a sensor — it's a sensor with an embedded processing engine that absorbs the majority of the ESP32's planned firmware workload:

- **Hardware SFLP (Sensor Fusion Low Power):** Outputs a game rotation vector as a quaternion directly on the sensor die. Zero ESP32 CPU cost for orientation tracking. Eliminates Madgwick/Mahony firmware entirely.
- **Hardware FSM (Finite State Machine × 8):** Configurable tap/double-tap/freefall/wake detection with hardware interrupts. Spike and catch detection happens at the sensor's native sample rate (up to 7.68 kHz) and fires an INT pin — no ESP32 polling loop.
- **Hardware MLC (Machine Learning Core):** Trainable decision tree engine for complex gesture classification (spike vs. cup catch vs. lunar vs. lighthouse) running on-sensor. Phase 2 feature, hardware ready now.
- **Qvar (Electrostatic Charge Sensing):** Detects electrostatic charge variation near embedded electrodes. Potential for non-contact proximity detection between ken spike and tama ana — "spike approach" as a continuous expressive parameter.
- **Adaptive Self-Configuration (ASC):** Sensor auto-adjusts range and ODR based on detected motion patterns without host intervention.
- **Impact force measurement from single sensor:** 4.5 KB onboard FIFO buffers raw samples; when FSM fires an impact interrupt, ESP32 reads FIFO backwards to extract peak acceleration magnitude. At ±16g range with 16-bit ADC, resolution is 0.49 mg/LSB — more than sufficient to distinguish gentle cup catches from hard spikes.

**Why not dual-IMU (LSM6DSV16X + ISM330DHCX) in the tama:**

- The ISM330DHCX's noise floor advantage (~15% lower noise density) is irrelevant for distinguishing impact events that differ by orders of magnitude in g-force.
- Adding a second I2C device doubles bus traffic, adds firmware complexity, increases power draw, and consumes physical space inside a 93mm hemisphere.
- The ISM330DHCX has no onboard gesture detection — adding it would reintroduce the firmware polling loop that the LSM6DSV16X's FSM eliminates.

### ISM330DHCX × 4: All Spares

Retained for bench testing, prototyping, or validating LSM6DSV16X SFLP accuracy. Available as fallback if LSM6DSV16X FSM tap detection proves insufficient for kendama impact signatures during testing.

### LEDs: APA102 DotStar (144 LED/m density strip, cut to size)

| Unit | LED Count | Strip Config | Frame Size |
|------|-----------|-------------|------------|
| Tama | 36 LEDs | Around equator | 4 + 144 + 3 = 151 bytes |
| Ken | 24 LEDs | 3–4 per cup × 3 cups | 4 + 96 + 2 = 102 bytes |

**Why APA102 (DotStar) over WS2812B (NeoPixel):**

- **Separate clock and data lines (SPI):** Not timing-critical, no interrupt-disable requirement. The WS2812B's single-wire 800 kHz protocol requires disabling interrupts during transmission, which conflicts with ESP-NOW callbacks and I2C bus activity on a shared core.
- **20 kHz PWM refresh rate** (no visible flicker at any brightness) versus WS2812B's 400 Hz (visible flickering at low brightness and during motion — critical for a device that is constantly in motion).
- **Up to 32 MHz SPI clock:** A full 36-LED frame at 8 MHz takes ~0.15 ms. The LED hardware is never the bottleneck.
- **Any processor speed or timing works:** No strict timing requirements means the ESP32 can service I2C and ESP-NOW between LED updates without corrupting the strip data.

### ESP32-C5 (SparkFun Thing Plus): Not Used

Saved for future projects. The C5's dual-band WiFi 6 radio is irrelevant to the ESP-NOW architecture, its single HP core cannot match the S3's dual-core real-time partitioning, its USB Serial/JTAG cannot present as a composite CDC device, and its active-mode power draw is equivalent to the S3. No advantage in any role.

---

## 7. Power Budget

### Per-LED Current

APA102 maximum draw per LED: 60 mA (20 mA per channel at full brightness R=255, G=255, B=255).

Uncapped worst case:

| Unit | LEDs | Max Current (all white, full brightness) |
|------|------|-----------------------------------------|
| Tama | 36 | 36 × 60 mA = **2.16 A** |
| Ken | 24 | 24 × 60 mA = **1.44 A** |
| **Total** | 60 | **3.60 A** |

This is dangerously high for battery operation and will exceed safe LiPo discharge rates, fry thin signal wiring, and drain the battery in under 30 minutes. **Firmware current capping is mandatory.**

### Firmware LED Current Limiter

Before every SPI frame write, the firmware calculates the total requested current for all LEDs:

```
frame_current_mA = sum_over_all_LEDs( (R/255 × 20) + (G/255 × 20) + (B/255 × 20) )
```

If `frame_current_mA` exceeds the configured ceiling, all LED values are proportionally scaled down:

```
scale_factor = max_allowed_mA / frame_current_mA
R_actual = R_requested × scale_factor
G_actual = G_requested × scale_factor
B_actual = B_requested × scale_factor
```

This preserves color ratios (hue stays the same) while enforcing a hard current ceiling.

### Current Budgets Per Unit

**Tama (1500 mAh LiPo):**

| Component | Current | Notes |
|-----------|---------|-------|
| ESP32-S3 (WiFi/ESP-NOW active) | 70 mA | Dual-core, radio active |
| LSM6DSV16X (continuous SFLP) | 1 mA | Hardware fusion running |
| APA102 LEDs (firmware-capped) | **250 mA** | Hard ceiling, enforced in firmware |
| 5V boost converter inefficiency (~85%) | 50 mA | Overhead on LED power path |
| **Total** | **~371 mA** | |

**Tama battery life:** 1500 mAh ÷ 371 mA ≈ **4.0 hours** (conservative)

**Ken (2000 mAh LiPo):**

| Component | Current | Notes |
|-----------|---------|-------|
| ESP32-S3 (WiFi/ESP-NOW active) | 70 mA | Dual-core, radio active |
| LSM6DSV16X (continuous SFLP) | 1 mA | Hardware fusion running |
| APA102 LEDs (firmware-capped) | **200 mA** | Hard ceiling, enforced in firmware |
| 5V boost converter inefficiency (~85%) | 40 mA | Overhead on LED power path |
| **Total** | **~311 mA** | |

**Ken battery life:** 2000 mAh ÷ 311 mA ≈ **6.4 hours** (conservative)

### Low Battery Behavior

- At 20% battery: LED current ceiling drops to 50% of configured max.
- At 10% battery: LEDs dim to minimal status indicator (single LED, low brightness).
- At 5% battery: LEDs off, sensor telemetry continues at reduced rate.
- Battery voltage monitored via ESP32-S3 ADC on the Feather's built-in battery divider.

### Wire Gauge Requirements

- **LED power lines (5V, GND):** 22 AWG silicone minimum (rated for 0.92A continuous). Adequate for firmware-capped current.
- **LED data lines (SPI CLK, MOSI):** 26–28 AWG silicone (signal only, minimal current).
- **I2C lines:** 26–28 AWG (signal only).
- **Battery to ESP32:** JST-PH (built into Feather, rated for 1A).
- **Battery to boost converter:** 22 AWG minimum, short runs (<50mm).

---

## 8. Latency Budget (Locked Targets)

### Discrete Gesture Events (Spike → Sample Trigger)

| Stage | Time | Hardware |
|-------|------|----------|
| LSM6DSV16X FSM interrupt → ESP32 ISR | 0.1 ms | Tama/Ken S3 |
| ESP32 reads event register + packs OSC | 0.1 ms | Tama/Ken S3 |
| ESP-NOW Tx (single hop, ~100 bytes) | 0.3 ms | 2.4 GHz air |
| Receiver Rx callback + SLIP encode | 0.1 ms | Receiver S3 |
| USB-Serial write (Port A) | 0.5 ms | USB Full Speed |
| Compiled bridge → UDP localhost | 0.05 ms | macOS |
| M4L `udpreceive` → `live.remote~` | 0.1 ms | Ableton audio thread |
| Audio buffer → RME DAC | 0.67 ms | 64 samples @ 96 kHz |
| **Total** | **~1.9 ms** | |

### Continuous Parameters (Tilt → Filter Sweep)

| Stage | Time | Hardware |
|-------|------|----------|
| SFLP quaternion output → ESP32 FIFO read | 0.2 ms | LSM6DSV16X + S3 |
| OSC pack + ESP-NOW Tx | 0.4 ms | S3 + 2.4 GHz air |
| Receiver → USB-Serial (Port A) → bridge → UDP | 0.6 ms | Receiver + macOS |
| M4L parse → parameter update | 0.2 ms | Ableton |
| Audio buffer → RME DAC | 0.67 ms | 64 samples @ 96 kHz |
| **Total** | **~2.1 ms** | |

### LED Response Latency (Varies by Source Layer)

| LED Source | Path | Latency |
|------------|------|---------|
| Layer 0: On-device IMU-reactive | ESP32 firmware → SPI | **<1 ms** |
| Layer 0.5: Receiver-coordinated | ESP-NOW → Receiver compute → ESP-NOW back | **~1.5 ms** |
| Layer 1: M4L music-reactive | M4L → Port B → Receiver → ESP-NOW → SPI | **~3 ms** |

---

## 9. LED Architecture: Priority-Layered Control

The APA102 hardware can accept frames at 6,600+ fps (151 bytes at 8 MHz SPI = 0.15 ms per frame). The 20 kHz PWM can visually resolve changes at up to 20,000 fps. **The LED hardware is never the bottleneck.** The update rate is determined by the data source.

### Critical Distinction: PWM Refresh Rate vs. Data Frame Rate

These are two independent properties that should not be conflated:

**PWM refresh rate (20 kHz, hardware-autonomous):** Each APA102 pixel has an internal oscillator that pulses the LED diode at 20,000 Hz to achieve the commanded brightness. Once a pixel receives a color command, it maintains that output continuously at 20 kHz until it receives a new command or loses power. This is completely decoupled from software. Even at 1 fps data rate, the physical light output is flicker-free. The CFL/fluorescent "strobe" effect (caused by 100–120 Hz mains-frequency flicker) is physically impossible with DotStars at any software update rate.

**Data frame rate (software-determined):** How often the ESP32 sends new color values to the strip via SPI. This determines animation smoothness, not flicker. A slow data rate (e.g., 60 fps) on a fast-spinning kendama produces chunky color stepping — the Persistence of Vision "dotted line" effect — because each pixel holds its color for 16.67 ms while the device rotates. At 500 Hz (Layer 0), each color step covers <3.6° of rotation at 5 rev/s, producing smooth continuous trails to the eye and on camera.

**Bottom line:** Flicker is a solved hardware problem (20 kHz PWM). Animation smoothness is a software frame rate problem, solved by Layer 0 local rendering at 500 Hz for motion-tracking effects.

The ESP32 firmware runs its SPI LED output at a fixed rate (200 Hz — one frame every 5 ms) and composites color data from multiple sources using a priority layering system:

### Layer 0 — On-Device IMU-Reactive (Highest Priority, ~500 Hz capable)

Generated entirely in firmware from local IMU data. Zero network involvement. Works with no computer connection.

**Examples:**
- Orientation-driven hue (tilt angle → color wheel position).
- Impact flash (FSM interrupt → full white for 50 ms, exponential decay).
- Spin-rate chase (gyro Z → speed of color rotation around strip).
- Low-battery warning pulse.

**Latency:** Sub-millisecond (IMU read → computation → SPI write, all on-chip).

**This layer is always running.** It is the base visual state of the instrument. If the laptop crashes mid-performance, the kendama still lights up reactively.

### Layer 0.5 — Receiver-Coordinated (Cross-Device Awareness, ~200 Hz capable)

Generated on the receiver ESP32-S3, which sees both devices' sensor streams in real time. The receiver computes coordinated LED frames and sends them back via ESP-NOW. **No laptop round-trip.**

**Examples:**
- Proximity color blending (as tama approaches ken, both strips shift toward a shared hue).
- Synchronized impact flash (spike lands → both devices flash simultaneously, receiver detects the correlation).
- Complementary color chase (tama and ken strips animate as a single continuous 60-LED ring, coordinated by the receiver).
- Relative orientation effects (color difference driven by the quaternion delta between tama and ken).

**Latency:** ~1.5 ms (ESP-NOW in from both devices → receiver computation → ESP-NOW back to both devices).

**This layer requires the receiver but not the laptop.** If Ableton isn't running, the instruments still coordinate visually.

### Layer 1 — M4L Music-Reactive (Audio-Driven, 100–200 Hz)

Generated in Max for Live from Ableton's audio analysis (beat detection, FFT spectrum, envelope followers) combined with both sensor streams (which M4L receives on `udpreceive 9000`). Sent as OSC LED commands via UDP to localhost:9001 → compiled bridge → USB-Serial Port B → receiver → ESP-NOW → devices.

**Examples:**
- Beat-synced pulses (downbeat → full brightness flash on both strips).
- Frequency-band color mapping (bass → red, mids → green, highs → blue).
- Envelope-driven brightness (audio volume → LED intensity).
- Song-specific choreographed patterns (saved as M4L presets per track).

**Latency:** ~3 ms (M4L computation → serial → receiver → ESP-NOW → SPI).

**This layer requires the laptop and Ableton running.**

### Layer Compositing

When multiple layers are active, higher-priority layers override lower-priority layers unless blending is explicitly configured. The firmware maintains a simple compositing model:

- **Override mode (default):** If Layer 1 data is arriving, it replaces Layer 0/0.5 entirely.
- **Blend mode (configurable):** Layer 1 is alpha-composited over Layer 0/0.5, allowing local reactive effects to show through music-reactive patterns.
- **Fallback:** If Layer 1 data stops arriving (M4L disconnected), firmware falls back to Layer 0.5 (if receiver is coordinating) or Layer 0 (always available).

### Why Not TouchDesigner

TD was previously designated for "complex generative LED visuals." This role is eliminated because:

- The LED output is 60 discrete RGB values on a 1D strip, not a 4K projection surface. The "visual complexity" required is color interpolation, sine functions, and lookup tables — trivially handled by M4L's `[js]` or `[gen~]` objects, or by firmware directly.
- TD's 60 fps cook-frame buffer would have been the slowest LED source in the system (16.67 ms worst-case). M4L runs at audio buffer rate (~1,500 Hz). Firmware runs at 500+ Hz.
- TD adds an entire application to launch, manage, and debug during performance. The entire software stack is now Ableton Live. One application.
- The "relative positioning" coordination that seemed to require a centralized visual engine is handled by the receiver firmware (Layer 0.5) at ~1.5 ms latency, far below TD's capabilities.

---

## 10. Software Architecture

### ESP32-S3 Firmware (Tama & Ken)

Each instrument runs identical firmware with a device-role flag (tama vs. ken) that sets the OSC address prefix.

**Core 0:** ESP-NOW stack (Tx/Rx), power management.
**Core 1:** I2C reads from LSM6DSV16X FIFO, OSC packet assembly, LED SPI output, LED layer compositing.

**Per-cycle firmware workload (with LSM6DSV16X doing onboard processing):**

- Read quaternion from SFLP FIFO: ~0.2 ms
- Read gesture interrupt flag (FSM): ~0.05 ms
- Read peak acceleration from FIFO on impact events: ~0.1 ms
- Pack OSC message: ~0.05 ms
- Signal Core 0 to transmit: ~0.01 ms
- Compute Layer 0 LED colors: ~0.1 ms
- Composite with incoming Layer 0.5/1 data (if present): ~0.05 ms
- SPI write to APA102 strip: ~0.15 ms
- **Total per cycle: ~0.7 ms** (at 200 Hz = 3.5% CPU utilization on Core 1)

### ESP32-S3 Firmware (Receiver)

**Responsibilities:**

1. Register two ESP-NOW peers (Tama MAC, Ken MAC).
2. On ESP-NOW Rx from instruments: SLIP-encode the OSC payload, write to USB-Serial **Port A**.
3. On USB-Serial **Port B** Rx from laptop: parse SLIP frames, route to target device via ESP-NOW.
4. Maintain latest sensor state for both devices (last-received orientation, gesture, etc.).
5. When Layer 0.5 is enabled: compute coordinated LED frames from both device states, transmit via ESP-NOW to both devices at 200 Hz.

**Firmware complexity:** ~300–400 lines (up from 150 in v1.0 due to Layer 0.5 coordination logic and dual USB-CDC).

### Laptop Software

**Compiled Bridge (C, ~80 lines):**

- Open Port A (read) and Port B (write) as separate serial file descriptors.
- Thread 1: Read SLIP frames from Port A, send as UDP to localhost:9000.
- Thread 2: Read UDP from localhost:9001, SLIP-encode, write to Port B.
- No heap allocation in the hot path. Deterministic latency.
- Built once, runs as a background daemon.

**Max for Live Device:**

- `udpreceive 9000` → OSC route by address prefix.
- `/kendama/tama/orientation` → continuous parameter mapping via `live.remote~`.
- `/kendama/tama/gesture` → clip triggers, sample launches.
- `/kendama/ken/...` → same, independent routing.
- LED pattern generator (audio analysis → RGB values) → OSC LED commands → `udpsend 9001`.
- Configuration sliders → OSC config commands → `udpsend 9001` → receiver → ESP-NOW → devices.

---

## 11. OSC Address Namespace

### Outbound (Instrument → Ableton via Receiver Port A)

```
/kendama/tama/orientation   [float qw] [float qx] [float qy] [float qz]
/kendama/tama/accel         [float x]  [float y]  [float z]    (m/s², raw)
/kendama/tama/gyro          [float x]  [float y]  [float z]    (°/s, raw)
/kendama/tama/spin_rate     [float rpm]
/kendama/tama/gesture       [string type] [float force]         (e.g., "spike", 14.2)
/kendama/tama/battery       [float 0.0–1.0]

/kendama/ken/orientation    [float qw] [float qx] [float qy] [float qz]
/kendama/ken/accel          [float x]  [float y]  [float z]
/kendama/ken/gyro           [float x]  [float y]  [float z]
/kendama/ken/gesture        [string type] [float force]
/kendama/ken/battery        [float 0.0–1.0]
```

### Inbound (M4L → Instruments via Receiver Port B)

```
/kendama/tama/led/rgb        [int R] [int G] [int B]           (0–255 each, global)
/kendama/tama/led/brightness [int 0–255]                       (global brightness)
/kendama/tama/led/pixel      [int index] [int R] [int G] [int B]  (per-pixel)
/kendama/tama/led/frame      [blob 108 bytes]                  (full 36-pixel frame, RGB)
/kendama/tama/led/pattern    [int pattern_id]                   (trigger firmware pattern)

/kendama/ken/led/rgb         [int R] [int G] [int B]
/kendama/ken/led/brightness  [int 0–255]
/kendama/ken/led/pixel       [int index] [int R] [int G] [int B]
/kendama/ken/led/frame       [blob 72 bytes]                   (full 24-pixel frame, RGB)
/kendama/ken/led/pattern     [int pattern_id]
```

### System / Configuration

```
/kendama/tama/calibrate      [bang]
/kendama/ken/calibrate       [bang]
/kendama/tama/config/curve   [int curve_id] [float param1] [float param2]
/kendama/ken/config/curve    [int curve_id] [float param1] [float param2]
/kendama/tama/config/led_cap [int max_mA]                      (runtime LED current limit)
/kendama/ken/config/led_cap  [int max_mA]
/kendama/tama/sleep          [bang]
/kendama/ken/sleep           [bang]
/kendama/receiver/layer05    [int 0|1]                          (enable/disable coordinated LED mode)
```

---

## 12. Contingency Notes

**If LSM6DSV16X FSM tap detection is unreliable for kendama impact forces:**
Fall back to ISM330DHCX for raw force measurement in the tama. Accept the firmware polling loop cost. The ISM330DHCX units are in stock as spares.

**If ESP-NOW packet loss exceeds 1% on stage:**
Increase Tx power to 20 dBm (if not already). Switch to a less congested 2.4 GHz channel. As last resort, fall back to WiFi UDP with a dedicated portable router.

**If compiled bridge introduces unexpected complexity:**
Fall back to M4L direct serial read via `[serial]` object. Benchmark serial polling rate. Alternatively, use the Python bridge for development and optimize later only if latency measurements justify it.

**If Max for Live `udpreceive` has polling latency issues:**
Investigate M4L `[udpreceive]` running in scheduler thread vs. audio thread. Alternatively, use a dedicated M4L external/Java object for lower-latency socket reads.

**If receiver Layer 0.5 coordination firmware becomes too complex:**
Disable Layer 0.5, move all coordinated LED logic to M4L (Layer 1). Accept the additional ~1.5 ms latency for coordinated effects. Layer 0 (on-device) remains unaffected.

**If APA102 SPI signal integrity degrades inside 3D-printed shell:**
Reduce SPI clock from 8 MHz to 4 MHz or 2 MHz. Frame time increases from 0.15 ms to 0.3 ms or 0.6 ms — still negligible at 200 Hz update rate. Ensure short wire runs (<150mm) and proper grounding.

---

## 13. Decision Log

| Decision | Rationale | Alternatives Rejected |
|----------|-----------|----------------------|
| ESP-NOW over WiFi infrastructure | No router, lower latency, connectionless, peer-to-peer | WiFi UDP (requires router, higher latency, association overhead) |
| OSC everywhere, no MIDI | Float precision, hierarchical namespace, self-documenting, no 7-bit limitation | USB-MIDI (7-bit, opaque CC numbers, channel limitations) |
| Hardwired USB receiver | Lowest latency (~1 ms), most reliable, simplest | Wireless receiver (adds WiFi hop, 2–4 ms, variability) |
| Dual USB-CDC serial ports | Upstream/downstream isolation, no bidirectional contention | Single serial port (LED writes could block sensor reads) |
| Compiled C bridge (primary) | Deterministic I/O, no GC/GIL, zero-allocation hot path | Python bridge (GIL risk, GC pauses — demoted to dev tool) |
| ESP32-S3 over ESP32-C5 | Dual symmetric cores, USB OTG/CDC composite, mature ecosystem, equivalent active power | ESP32-C5 (single HP core, no USB OTG, RISC-V sleep efficiency irrelevant during active performance) |
| LSM6DSV16X as sole IMU | Onboard SFLP/FSM/MLC/Qvar eliminates ESP32 firmware complexity | Dual-IMU with ISM330DHCX (marginal precision gain, double complexity) |
| APA102 DotStar over WS2812B NeoPixel | SPI (non-timing-critical), 20 kHz PWM, no interrupt conflicts | WS2812B (single-wire timing-critical, 400 Hz PWM, interrupt conflicts with ESP-NOW) |
| Parallel clients over serial daisy-chain | Failure isolation, no serialization dependency, simpler firmware | Daisy-chain (correlated failures, +1.7 ms avg latency, firmware complexity) |
| No TouchDesigner | M4L handles audio-reactive LEDs natively; receiver handles cross-device coordination; one less application | TD (60 fps cook bottleneck, extra app to manage, unnecessary for 60-LED 1D strip) |
| Priority-layered LED control | Graceful degradation (local → coordinated → music-reactive), sub-ms base response | Single-source LED control (fragile, no fallback, unnecessary latency for simple effects) |
| Firmware LED current capping | Prevents LiPo over-discharge, wiring damage, and thermal issues | No cap (dangerous: 3.6A theoretical max exceeds safe battery/wiring limits) |

---

*Document Version: 1.2*
*Locked: May 27, 2026*
*Author: Architecture audit collaboration (human + Claude)*
*Next: Part 2 — Firmware architecture, LSM6DSV16X SFLP/FSM configuration, ESP-NOW packet structure, SLIP-OSC serial protocol, compiled bridge implementation*
