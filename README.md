# Music Kendama

An open-source stringless jumbo kendama with embedded IMU sensors, addressable LEDs, and wireless OSC communication for real-time musical performance control in Ableton Live.

**Status:** Active development — architecture locked, firmware in progress.

Built in collaboration with [Claude](https://claude.ai) by Anthropic.

## What Is This?

A 25cm jumbo kendama (1.48× scaled Terra Prefect geometry) with an LSM6DSV16X IMU and APA102 DotStar LEDs in both the ken and tama. Motion data streams wirelessly via ESP-NOW to a USB receiver, through a compiled SLIP-OSC bridge, and into Ableton Live via Max for Live — all in under 2ms. No MIDI anywhere. No WiFi router. No TouchDesigner in the signal chain.

The kendama becomes a musical instrument: tilt controls filter sweeps, spin drives arpeggiation, spikes trigger samples, and the LEDs react to both motion and music in real time.

→ [Full architecture spec](docs/architecture/system-architecture-v1.2.md)

## Project Status

| Component | Status |
|-----------|--------|
| System architecture | ✅ Locked (v1.2) |
| SLIP-OSC bridge | ⬜ Not started |
| Receiver firmware | ⬜ Not started |
| Instrument firmware | ⬜ Not started |
| Max for Live device | ⬜ Not started |
| CAD | ⬜ Not started |
| 3D printing | ⬜ Not started |

## Architecture Overview

```
┌─ TAMA ──────────────────────────────────┐
│  LSM6DSV16X (fusion, gestures, Qvar)    │
│  ESP32-S3 (radio + LEDs)                │
│  APA102 DotStar strip (36 LEDs)         │
│  LiPo battery                           │
└──── ESP-NOW 2.4GHz ─────────────────────┘
                    ↓
┌─ USB RECEIVER ──────────────────────────┐
│  ESP32-S3                                │
│  Dual USB-CDC serial (up + down)         │
│  Coordinated LED computation             │
└──── USB-C to laptop ────────────────────┘
                    ↑
┌─ KEN ───────────────────────────────────┐
│  LSM6DSV16X (fusion, gestures, Qvar)    │
│  ESP32-S3 (radio + LEDs)                │
│  APA102 DotStar strip (24 LEDs)         │
│  LiPo battery                           │
└──── ESP-NOW 2.4GHz ─────────────────────┘

Laptop: Compiled C bridge → UDP localhost → Max for Live → Ableton Live → RME Babyface Pro
```

**Protocol at every hop:** OSC. Nothing else.

## Hardware

- 4× Adafruit ESP32-S3 Feather (ID: 5323)
- 2× SparkFun LSM6DSV16X 6DoF IMU Breakout (SEN-21336)
- APA102 DotStar LED strips (144 LED/m, cut to 36 + 24)
- LiPo batteries (1500 mAh tama, 2000 mAh ken)
- Ableton Live Suite 12 + Max for Live
- RME Babyface Pro FS (96 kHz, 64-sample buffer)

## Repository Structure

```
music-kendama/
├── docs/              System architecture, decision records, images
├── firmware/
│   ├── instrument/    ESP32-S3 firmware for tama and ken
│   ├── receiver/      ESP32-S3 firmware for USB receiver
│   └── shared/        Shared code (OSC packing, SLIP framing)
├── bridge/            Compiled C SLIP-to-UDP bridge
├── maxforlive/        Max for Live devices for Ableton
├── cad/               OpenSCAD source files and build instructions
└── tools/             Development and debugging utilities
```

## Building

Each component has its own build instructions in its directory README. Firmware uses PlatformIO. The bridge uses Make. CAD uses OpenSCAD.

### Quick Start (firmware)

```bash
cd firmware/receiver
cp include/secrets.h.template include/secrets.h
# Edit secrets.h with your ESP32 MAC addresses
pio run -t upload
```

## License

GPL-3.0. See [LICENSE](LICENSE).

## Contributing

This project is in early development. Issues and discussions welcome. Pull requests accepted once the core firmware stabilizes.

## Acknowledgments

This project is being developed in collaboration with Anthropic's Claude as an exploration of AI-assisted hardware design. Claude contributed to the system architecture, component selection, and firmware planning. The human brings the kendama expertise, performance vision, and makes all final decisions.
