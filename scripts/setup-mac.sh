#!/bin/bash
# setup-mac.sh — One-time development environment setup for Music Kendama
# Run this once on each development machine.

set -e

echo "=== Music Kendama — macOS Development Setup ==="
echo ""

# Check for Homebrew
if ! command -v brew &> /dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "✓ Homebrew already installed"
fi

# PlatformIO Core (CLI)
if ! command -v pio &> /dev/null; then
    echo "Installing PlatformIO Core..."
    brew install platformio
else
    echo "✓ PlatformIO already installed ($(pio --version))"
fi

# libserialport (for the compiled bridge)
if brew list libserialport &> /dev/null 2>&1; then
    echo "✓ libserialport already installed"
else
    echo "Installing libserialport..."
    brew install libserialport
fi

# Python packages
echo "Installing Python packages..."
pip3 install --quiet pyserial python-osc

# Verify installations
echo ""
echo "=== Verification ==="
echo "PlatformIO: $(pio --version 2>/dev/null || echo 'NOT FOUND')"
echo "libserialport: $(brew list --versions libserialport 2>/dev/null || echo 'NOT FOUND')"
echo "Python pyserial: $(pip3 show pyserial 2>/dev/null | grep Version || echo 'NOT FOUND')"
echo "Python python-osc: $(pip3 show python-osc 2>/dev/null | grep Version || echo 'NOT FOUND')"
echo "Git: $(git --version 2>/dev/null || echo 'NOT FOUND')"

# Remind about manual steps
echo ""
echo "=== Manual Steps Remaining ==="
echo ""
echo "1. Create secrets files:"
echo "   cp firmware/instrument/include/secrets.h.template firmware/instrument/include/secrets.h"
echo "   cp firmware/receiver/include/secrets.h.template firmware/receiver/include/secrets.h"
echo "   Then edit both with your ESP32 MAC addresses."
echo ""
echo "2. Create local environment file:"
echo "   cp .env.local.example .env.local"
echo ""
echo "3. Add shell alias to ~/.zshrc:"
echo "   alias kendama-bridge='~/Projects/music-kendama/bridge/music-kendama-bridge \$(ls /dev/tty.usbmodem* 2>/dev/null | head -1)'"
echo ""
echo "4. Install Google Drive desktop app (if not already) and set"
echo "   'Music Kendama' folder to 'Available offline'."
echo ""
echo "=== Setup complete ==="
