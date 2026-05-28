#!/bin/bash
# setup-mac.sh — One-time development environment setup for Music Kendama
#
# Sets up a fresh macOS machine for Music Kendama development:
#   - Installs Homebrew, PlatformIO, libserialport, Python deps
#   - Creates secrets.h files from templates (if missing)
#   - Creates .env.local from example (if missing)
#   - Installs a path-aware kendama-bridge shell alias
#
# Safe to run multiple times — checks before installing/overwriting.
# Run from the repo root: ./scripts/setup-mac.sh

set -euo pipefail

# Resolve repo root from this script's location (works regardless of clone path)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

echo "=== Music Kendama — macOS Development Setup ==="
echo "Repo root: $REPO_ROOT"
echo ""

# --- Homebrew ---------------------------------------------------------------
if ! command -v brew &> /dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    # Add to PATH for Apple Silicon
    if [[ -x /opt/homebrew/bin/brew ]]; then
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
else
    echo "✓ Homebrew already installed"
fi

# --- PlatformIO -------------------------------------------------------------
if ! command -v pio &> /dev/null; then
    echo "Installing PlatformIO Core..."
    brew install platformio
else
    echo "✓ PlatformIO already installed ($(pio --version))"
fi

# --- libserialport (compiled bridge dependency) -----------------------------
if brew list libserialport &> /dev/null 2>&1; then
    echo "✓ libserialport already installed"
else
    echo "Installing libserialport..."
    brew install libserialport
fi

# --- Python packages (dev tools) --------------------------------------------
echo "Installing Python packages (pyserial, python-osc)..."
pip3 install --quiet --break-system-packages pyserial python-osc 2>/dev/null \
    || pip3 install --quiet pyserial python-osc

# --- Secrets files ----------------------------------------------------------
echo ""
echo "--- Config files ---"
for target in firmware/instrument firmware/receiver; do
    if [[ -f "$target/include/secrets.h" ]]; then
        echo "✓ $target/include/secrets.h exists (not overwriting)"
    else
        cp "$target/include/secrets.h.template" "$target/include/secrets.h"
        echo "→ Created $target/include/secrets.h — EDIT with your MAC addresses"
    fi
done

# --- .env.local -------------------------------------------------------------
if [[ -f .env.local ]]; then
    echo "✓ .env.local exists (not overwriting)"
else
    cp .env.local.example .env.local
    echo "→ Created .env.local"
fi

# --- Shell alias (path-aware) -----------------------------------------------
ALIAS_LINE="alias kendama-bridge='$REPO_ROOT/bridge/music-kendama-bridge \$(ls /dev/tty.usbmodem* 2>/dev/null | head -1)'"
ZSHRC="$HOME/.zshrc"
if grep -q "alias kendama-bridge=" "$ZSHRC" 2>/dev/null; then
    echo "✓ kendama-bridge alias already in ~/.zshrc"
else
    echo "" >> "$ZSHRC"
    echo "# Music Kendama bridge launcher (auto-added by setup-mac.sh)" >> "$ZSHRC"
    echo "$ALIAS_LINE" >> "$ZSHRC"
    echo "→ Added kendama-bridge alias to ~/.zshrc (restart shell to use)"
fi

# --- Verification -----------------------------------------------------------
echo ""
echo "=== Verification ==="
printf "%-18s %s\n" "Homebrew:"     "$(brew --version 2>/dev/null | head -1 || echo 'NOT FOUND')"
printf "%-18s %s\n" "PlatformIO:"   "$(pio --version 2>/dev/null || echo 'NOT FOUND')"
printf "%-18s %s\n" "libserialport:" "$(brew list --versions libserialport 2>/dev/null || echo 'NOT FOUND')"
printf "%-18s %s\n" "pyserial:"     "$(pip3 show pyserial 2>/dev/null | grep Version || echo 'NOT FOUND')"
printf "%-18s %s\n" "python-osc:"   "$(pip3 show python-osc 2>/dev/null | grep Version || echo 'NOT FOUND')"
printf "%-18s %s\n" "git:"          "$(git --version 2>/dev/null || echo 'NOT FOUND')"

echo ""
echo "=== Remaining manual steps ==="
echo "1. Edit secrets.h files with your ESP32 MAC addresses:"
echo "     firmware/instrument/include/secrets.h"
echo "     firmware/receiver/include/secrets.h"
echo "2. Set git identity (if not already global):"
echo "     git config --global user.name \"miles ornish\""
echo "     git config --global user.email \"mornish@gmail.com\""
echo "3. Install Google Drive desktop app; set 'Music Kendama' to 'Available offline'."
echo "4. Restart your shell (or run: source ~/.zshrc) to pick up the alias."
echo ""
echo "=== Setup complete ==="
