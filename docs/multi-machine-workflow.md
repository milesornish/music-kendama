# Multi-Machine Development Workflow

Guidelines for working seamlessly across multiple development machines.

**Current setup:** MacBook Pro (M5 Pro, home/primary) + MacBook Air (M5, daily carry).

---

## Initial Setup (Run Once Per Machine)

### 1. Install Development Tools

Run the setup script from the repo root:

```bash
./scripts/setup-mac.sh
```

Or install manually:

```bash
# Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# PlatformIO Core (CLI)
brew install platformio

# Bridge dependencies
brew install libserialport

# Python packages (for dev tools)
pip3 install pyserial python-osc

# Git (should already be present on macOS)
brew install git
```

### 2. Clone the Repo

```bash
cd ~/Projects  # or wherever you keep code
git clone https://github.com/milesornish/music-kendama.git
cd music-kendama
```

### 3. Create Secrets Files

```bash
cp firmware/instrument/include/secrets.h.template firmware/instrument/include/secrets.h
cp firmware/receiver/include/secrets.h.template firmware/receiver/include/secrets.h
```

Edit both `secrets.h` files with your ESP32 MAC addresses. These are the same on both machines (they're properties of the ESP32 boards, not the computer).

### 4. Create Local Environment File

```bash
cp .env.local.example .env.local
```

Edit `.env.local` with any machine-specific preferences. This file is gitignored.

### 5. Verify Build Tools

```bash
# Firmware builds
cd firmware/receiver && pio run && cd ../..

# Bridge builds
cd bridge && make && cd ..
```

PlatformIO will download the ESP32 platform packages automatically on first run. This takes a few minutes but only happens once.

---

## What Lives Where

### Git Repo (syncs via push/pull)

Everything that is source code or documentation:

- All firmware source (`firmware/`)
- Bridge source (`bridge/`)
- Dev tools and scripts (`tools/`, `scripts/`)
- Architecture docs (`docs/`)
- PlatformIO configs (`platformio.ini`)
- Makefiles
- Secrets templates (`.template` files)
- Released M4L devices (`maxforlive/`)
- This workflow guide

### Google Drive (syncs automatically)

Everything that is binary, creative, or project management:

- Ableton Live sets (`.als`) and audio samples
- Working M4L device copies (`.amxd`)
- Reference datasheets (PDFs)
- Purchased component receipts
- Photos of builds and prototypes
- Google Docs (Master Document, setup guides)
- Google Sheets (Budget, BOM, Print Log, Inventory)

**Recommended Google Drive structure:**

```
Google Drive/
└── Music Kendama/
    ├── Ableton/
    │   ├── Music Kendama Live Set/    (Ableton project folder)
    │   └── Samples/
    ├── Datasheets/
    ├── Photos/
    │   ├── Build Progress/
    │   └── Bench Setup/
    └── Receipts/
```

### Machine-Local (never syncs, never needs to)

- `secrets.h` (created once per machine from template, same content)
- `.env.local` (machine-specific preferences)
- `.pio/` (PlatformIO build cache, regenerated automatically)
- Compiled bridge binary (rebuilt from source in seconds)

---

## The Daily Workflow

### Ending a Session (Critical)

Before closing the laptop lid:

```bash
git add -A
git status                    # review what's being committed
git commit -m "WIP: [what you were working on]"
git push
```

This is the single most important habit. An unsynchronized repo is the only thing that creates real friction between machines. A `WIP:` commit that doesn't compile is infinitely better than clean code stuck on the wrong laptop. Squash the WIP into a proper commit when you finish the feature.

### Starting a Session

When you open a laptop to work:

```bash
cd ~/Projects/music-kendama
git pull
```

That's it. Everything else is already configured from the one-time setup.

### Building and Testing

```bash
# Firmware (auto-detects connected board)
cd firmware/receiver
pio run -t upload

# Bridge (auto-discovers serial port via alias)
cd bridge
make
kendama-bridge    # shell alias, see below
```

---

## Serial Port Handling

macOS assigns different device paths to the same USB board on different machines (e.g., `/dev/tty.usbmodem14101` on the Pro, `/dev/tty.usbmodem14201` on the Air). Never hardcode serial port paths.

### Bridge Auto-Discovery

Add to your `~/.zshrc` on both machines:

```bash
# Music Kendama bridge launcher
# Auto-discovers ESP32-S3 Feather by scanning USB modem devices
alias kendama-bridge='~/Projects/music-kendama/bridge/music-kendama-bridge $(ls /dev/tty.usbmodem* 2>/dev/null | head -1)'
```

### PlatformIO Upload

Leave `upload_port` and `monitor_port` unset in `platformio.ini`. PlatformIO auto-detects connected boards by USB VID/PID. Override on the command line only if needed:

```bash
pio run -t upload --upload-port /dev/tty.usbmodemXXXX
```

---

## Ableton Live Coordination

### Google Drive Sync for Live Sets

The Ableton project folder lives in Google Drive, which syncs automatically across both machines. Both machines must have the Google Drive desktop app installed with the Music Kendama folder set to "Available offline" for reliable access.

### Avoid Simultaneous Edits

Never have the same Live set open on both machines at the same time. Google Drive handles sequential edits fine but creates conflict copies on simultaneous writes. Close Ableton before switching machines.

### M4L Device Versioning

Your working M4L device lives in the Ableton project folder (synced via Google Drive). When you reach a stable version, copy it into the git repo's `maxforlive/` directory and commit it. The repo holds released snapshots; Google Drive holds the working copy.

---

## Troubleshooting

**"I forgot to push before leaving"**

You have two options: work on something else (docs, CAD, a different feature branch) until you can push from the other machine, or SSH into the home machine if it's awake and push remotely. The best fix is building the push habit.

**"PlatformIO is downloading everything again"**

The `.pio/` build cache is machine-local and gitignored. First build on a fresh clone takes a few minutes to download the ESP32 platform. Subsequent builds are fast. This is normal and expected.

**"Serial port not found"**

Check that the ESP32 is plugged in and recognized: `ls /dev/tty.usbmodem*`. If nothing shows, try a different USB-C cable (some are charge-only). The Adafruit Feather S3 needs a data-capable cable.

**"Ableton can't find samples"**

Make sure the Google Drive folder is set to "Available offline" on both machines. If samples are missing, Ableton will show them as offline — right-click and relocate to the Google Drive path.
