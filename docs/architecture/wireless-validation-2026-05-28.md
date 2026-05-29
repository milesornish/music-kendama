# Wireless Core — Bench Validation Log (2026-05-28)

First hardware bring-up of the Music Kendama wireless core. All results below were
observed on real ESP32-S3 hardware; this log records what is **proven** vs. **still
assumed**, to keep the locked v1.2 spec honest. It does not modify that spec.

## Setup
- 4× Adafruit ESP32-S3 Feather (no-PSRAM, 5323). MACs (efuse-validated):
  - `B4:3A:45:33:B9:70` (#1) · `B4:3A:45:34:0D:20` (#2) · `B4:3A:45:34:0C:E0` (#3) · `B4:3A:45:34:0D:80` (#4)
- IMU: **ISM330DHCX** (spec's documented spare/stand-in; the production LSM6DSV16X was not on hand).
- Stock Arduino-ESP32 2.0.x (IDF 4.4.1) via PlatformIO `espressif32`. Bench, indoor, <1 m, channel 1.
- Test firmware lives under `firmware/{bringup,dual-cdc-test,espnow-test,two-receiver-test}/` (spikes, not production).

## Proven
1. **Toolchain → build → flash → run → serial** end-to-end on the S3.
2. **Dual USB-CDC on stock Arduino: NOT possible.** The core's `tusb_config.h` hardcodes
   `CFG_TUD_CDC = 1` and overrides a `-DCFG_TUD_CDC=2` build flag (compile warning); on hardware
   a second `USBCDC` instance never enumerates (one port, "PORT A" only). See `firmware/dual-cdc-test/`.
3. **ESP-NOW link** (1 instrument → 1 receiver): unicast ACKed, 0 loss in a short run.
4. **IMU → ESP-NOW → receiver**: live ISM330DHCX accel/gyro streamed at 10 Hz, 0 loss.
5. **Two-receiver topology (split by direction)**: both directions simultaneously through two
   separate single-CDC chips — upstream (instrument → Receiver-UP) and downstream
   (laptop → Receiver-DOWN → instrument). Sidesteps the dual-CDC limit with stock Arduino. See `firmware/two-receiver-test/`.
6. **Parallel clients (§2)**: two instruments → one Receiver-UP, 30 s soak —
   **~591 packets, 0 missed, 9.9 pkt/s each, 0.00% loss**, demuxed per source MAC.

## The dual-CDC decision (honest options)
The receiver needs Port A (upstream) / Port B (downstream). Paths, by confidence:
- **Two single-CDC receivers (split by direction) — PROVEN here.** Simplest firmware (stock Arduino).
  Cost: a 2nd USB dongle, and Layer 0.5 coordination needs care (see below).
- **One chip, ESP-IDF + `esp_tinyusb` `CONFIG_TINYUSB_CDC_COUNT=2`** — standard, **not built/tested here**.
- **One chip, Arduino-ESP32 3.x (pioarduino fork)** — **untested**; might allow 2 CDC and keep a single framework.

## Open question
- **Layer 0.5** (on-receiver coordinated LED, §9): with receivers split by direction, the chip that
  has both sensor streams (UP) isn't the one that sends LEDs (DOWN). Options: let Receiver-UP also Tx
  coordinated frames, or push coordination to the laptop (Layer 1, +latency). Not resolved.

## NOT yet validated
- Rates above 10 Hz (LED frames target ~200 Hz) — zero-loss at 10 Hz does **not** prove it at 200 Hz.
- Range / RF interference / stage conditions; long-duration & thermal stability.
- The production **LSM6DSV16X** IMU, **APA102 DotStar** LEDs, the laptop **bridge**, and **M4L → Ableton**.

## Hardware quirks learned (Adafruit S3 Feather, native USB)
- First flash needs **manual download mode** (hold BOOT, tap RESET, release BOOT); esptool's auto-reset
  is unreliable. A physical **RESET tap** is needed to run the app after flashing.
- **Opening the serial port resets the board** (even with DTR/RTS low) — measure by opening only the
  port under test.
- **USB port numbers are unstable** across re-enumeration (worse via a display hub). Solution used:
  **self-roling firmware** — one binary; each board picks its role from its own MAC at boot.
