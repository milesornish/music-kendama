#!/bin/bash
# setup-mac.sh — Dev-environment setup + parity doctor for Music Kendama
#
# Two modes:
#   ./scripts/setup-mac.sh            Install everything automatable, then run the doctor.
#   ./scripts/setup-mac.sh --verify   Doctor only: print a parity snapshot, install nothing.
#                                     (aliases: verify, doctor, -v)
#
# What it AUTOMATES (install mode):
#   - Homebrew + CLI toolchain: platformio, libserialport, node, gh
#   - Python deps: pyserial, python-osc
#   - git identity (only if unset), gh auth detection
#   - secrets.h / .env.local from templates (never overwrites)
#   - path-aware kendama-bridge shell alias
#
# What it CANNOT install (licensed/GUI installers — it detects + guides instead):
#   - Ableton Live Suite (bundles Max for Live)
#   - RME Babyface Pro FS DriverKit driver + TotalMix FX
#   - TouchDesigner
#
# The doctor output is stable/sorted so two machines can be diffed for parity.
# Safe to run repeatedly.

set -euo pipefail

# --- Pinned versions for the manual GUI installs (bump here when they change) ---
RME_DRIVER="DriverKit v4.27  (NOT the v3.35 KEXT — that one forces Reduced Security)"
RME_URL="https://www.rme-usa.com/downloads.html"
TD_VERSION="2025.32820"
TD_URL="https://derivative.ca/download"
ABLETON_URL="https://www.ableton.com/en/account/"

# --- Personal git identity (set only if unset) ---
GIT_NAME="miles ornish"
GIT_EMAIL="mornish@gmail.com"

# --- Resolve repo root from this script's location (works regardless of clone path) ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# --- Mode ---
MODE="install"
case "${1:-}" in
    --verify|verify|doctor|-v) MODE="verify" ;;
    --help|-h)
        grep '^#' "$0" | sed 's/^# \{0,1\}//'
        exit 0 ;;
    "") ;;
    *) echo "Unknown argument: $1 (try --help)"; exit 2 ;;
esac

# --- Output helpers ---
ok()   { printf "  \033[32m✓\033[0m %s\n" "$1"; }
warn() { printf "  \033[33m⚠\033[0m %s\n" "$1"; }
miss() { printf "  \033[31m✗\033[0m %s\n" "$1"; }
have() { command -v "$1" &>/dev/null; }

# Detect a *.app bundle by glob; echoes its basename or "MISSING".
app_status() {
    local match
    match=$(ls -d "$1" 2>/dev/null | head -1 || true)
    if [[ -n "$match" ]]; then basename "$match"; else echo "MISSING"; fi
}

# ============================================================================
# INSTALL
# ============================================================================
install_all() {
    echo "=== Music Kendama — macOS Setup (install) ==="
    echo "Repo root: $REPO_ROOT"
    echo ""

    # --- Homebrew ---
    if ! have brew; then
        echo "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        [[ -x /opt/homebrew/bin/brew ]] && eval "$(/opt/homebrew/bin/brew shellenv)"
    else
        ok "Homebrew already installed"
    fi

    # --- brew CLI formulae ---
    echo "--- Homebrew formulae ---"
    for f in platformio libserialport node gh; do
        if brew list "$f" &>/dev/null; then
            ok "$f already installed"
        else
            echo "  Installing $f..."
            brew install "$f"
        fi
    done

    # --- Python packages ---
    echo "--- Python packages ---"
    if pip3 install --quiet --break-system-packages pyserial python-osc 2>/dev/null \
        || pip3 install --quiet pyserial python-osc; then
        ok "pyserial, python-osc"
    fi

    # --- git identity (only if unset) ---
    echo "--- git identity ---"
    if [[ -z "$(git config --global user.name || true)" ]]; then
        git config --global user.name "$GIT_NAME"
        ok "set user.name = $GIT_NAME"
    else
        ok "user.name already set ($(git config --global user.name))"
    fi
    if [[ -z "$(git config --global user.email || true)" ]]; then
        git config --global user.email "$GIT_EMAIL"
        ok "set user.email = $GIT_EMAIL"
    else
        ok "user.email already set ($(git config --global user.email))"
    fi

    # --- gh auth (detect only — login is interactive) ---
    if have gh && gh auth status &>/dev/null; then
        ok "gh authenticated"
    else
        warn "gh not authenticated — run:  gh auth login"
    fi

    # --- Config files (never overwrite) ---
    echo "--- Config files ---"
    for target in firmware/instrument firmware/receiver; do
        if [[ -f "$target/include/secrets.h" ]]; then
            ok "$target/include/secrets.h exists (not overwriting)"
        else
            cp "$target/include/secrets.h.template" "$target/include/secrets.h"
            warn "Created $target/include/secrets.h — EDIT with your MAC addresses"
        fi
    done
    if [[ -f .env.local ]]; then
        ok ".env.local exists (not overwriting)"
    else
        cp .env.local.example .env.local
        ok "Created .env.local"
    fi

    # --- Shell alias (path-aware) ---
    echo "--- Shell alias ---"
    local alias_line zshrc
    alias_line="alias kendama-bridge='$REPO_ROOT/bridge/music-kendama-bridge \$(ls /dev/tty.usbmodem* 2>/dev/null | head -1)'"
    zshrc="$HOME/.zshrc"
    if grep -q "alias kendama-bridge=" "$zshrc" 2>/dev/null; then
        ok "kendama-bridge alias already in ~/.zshrc"
    else
        {
            echo ""
            echo "# Music Kendama bridge launcher (auto-added by setup-mac.sh)"
            echo "$alias_line"
        } >> "$zshrc"
        warn "Added kendama-bridge alias to ~/.zshrc (run: source ~/.zshrc)"
    fi
    echo ""
}

# ============================================================================
# DOCTOR  (stable/sorted output — diff two machines for parity)
# ============================================================================
doctor() {
    echo "=== PARITY SNAPSHOT — $(scutil --get ComputerName 2>/dev/null || echo '?') ==="
    echo "macOS $(sw_vers -productVersion 2>/dev/null) ($(sw_vers -buildVersion 2>/dev/null))"

    echo ""
    echo "--- CLI toolchain ---"
    printf "  %-12s %s\n" "brew"       "$(brew --version 2>/dev/null | head -1 || echo MISSING)"
    printf "  %-12s %s\n" "node"       "$(node --version 2>/dev/null || echo MISSING)"
    printf "  %-12s %s\n" "npm"        "$(npm --version 2>/dev/null || echo MISSING)"
    printf "  %-12s %s\n" "gh"         "$(gh --version 2>/dev/null | head -1 || echo MISSING)"
    printf "  %-12s %s\n" "git"        "$(git --version 2>/dev/null || echo MISSING)"
    printf "  %-12s %s\n" "pio"        "$(pio --version 2>/dev/null || echo MISSING)"
    printf "  %-12s %s\n" "libserial"  "$(brew list --versions libserialport 2>/dev/null || echo MISSING)"
    printf "  %-12s %s\n" "pyserial"   "$(pip3 show pyserial 2>/dev/null | awk '/^Version/{print $2}' || echo MISSING)"
    printf "  %-12s %s\n" "python-osc" "$(pip3 show python-osc 2>/dev/null | awk '/^Version/{print $2}' || echo MISSING)"

    echo ""
    echo "--- Identity & auth ---"
    printf "  %-12s %s\n" "git.name"  "$(git config --global user.name  2>/dev/null || echo UNSET)"
    printf "  %-12s %s\n" "git.email" "$(git config --global user.email 2>/dev/null || echo UNSET)"
    if have gh && gh auth status &>/dev/null; then
        ok "gh authenticated ($(gh auth status 2>&1 | awk '/Logged in/{print $(NF-1); exit}'))"
    else
        miss "gh NOT authenticated"
    fi

    echo ""
    echo "--- Licensed GUI apps ---"
    printf "  %-12s %s\n" "Ableton"   "$(app_status '/Applications/Ableton Live'*.app)"
    printf "  %-12s %s\n" "TouchDes." "$(app_status '/Applications/TouchDesigner'*.app)"
    printf "  %-12s %s\n" "TotalMix"  "$(app_status /Applications/[Tt]otal[Mm]ix*.app)"

    echo ""
    echo "--- RME driver & security ---"
    local rme
    rme=$(systemextensionsctl list 2>/dev/null | grep -i "rme" || true)
    if [[ -z "$rme" ]]; then
        miss "no RME DriverKit extension present"
    elif echo "$rme" | grep -qi "enabled"; then
        ok "RME DriverKit extension: activated enabled"
    else
        warn "RME DriverKit extension present but NOT enabled — approve it in"
        warn "  System Settings → General → Login Items & Extensions"
    fi
    if kmutil showloaded 2>/dev/null | grep -qiE "rme|fireface"; then
        warn "old RME KEXT is loaded — you're on the legacy driver (reduced-security path)"
    else
        ok "no legacy RME KEXT loaded"
    fi
    printf "  %-12s %s\n" "SIP" "$(csrutil status 2>/dev/null | sed 's/System Integrity Protection status: //')"
    if system_profiler SPUSBDataType 2>/dev/null | grep -qiE "babyface|fireface"; then
        ok "Babyface detected on USB"
    else
        warn "Babyface not detected on USB (plug it in to confirm enumeration)"
    fi

    echo ""
    echo "--- Repo ---"
    printf "  %-12s %s\n" "branch" "$(git branch --show-current 2>/dev/null || echo '?')"
    printf "  %-12s %s\n" "head"   "$(git log -1 --oneline 2>/dev/null || echo '?')"
    git fetch -q 2>/dev/null || true
    printf "  %-12s %s\n" "vs origin" \
        "$(git rev-list --count HEAD..origin/main 2>/dev/null || echo '?') behind, $(git rev-list --count origin/main..HEAD 2>/dev/null || echo '?') ahead"
    printf "  %-12s %s\n" "secrets.h" \
        "$([[ -f firmware/instrument/include/secrets.h && -f firmware/receiver/include/secrets.h ]] && echo present || echo MISSING)"
    printf "  %-12s %s\n" "alias" \
        "$(grep -q 'alias kendama-bridge=' "$HOME/.zshrc" 2>/dev/null && echo present || echo MISSING)"
}

# ============================================================================
# MANUAL-STEPS GUIDE  (the irreducible, licensed/GUI installs)
# ============================================================================
guide() {
    echo ""
    echo "=== Remaining manual steps (can't be scripted) ==="
    echo ""
    echo "1. Ableton Live Suite  →  $ABLETON_URL"
    echo "   Download Live 12 Suite (Apple Silicon), install, authorize online."
    echo "   Suite bundles Max for Live — no separate M4L install. Let content packs"
    echo "   download once so M4L works offline."
    echo ""
    echo "2. RME Babyface Pro FS  →  $RME_URL"
    echo "   Install: $RME_DRIVER"
    echo "   The package also installs TotalMix FX. After install macOS BLOCKS the"
    echo "   driver extension — enable it under:"
    echo "       System Settings → General → Login Items & Extensions  (RME GmbH)"
    echo "   DriverKit needs NO Recovery Mode / Reduced Security. Keep SIP enabled."
    echo ""
    echo "3. TouchDesigner  →  $TD_URL"
    echo "   Download Apple Silicon build (v$TD_VERSION+), pick Non-Commercial (free)."
    echo ""
    echo "4. Google Drive desktop app — set 'Music Kendama' to 'Available offline'."
    echo ""
    echo "5. Edit secrets.h with your ESP32 MAC addresses:"
    echo "       firmware/instrument/include/secrets.h"
    echo "       firmware/receiver/include/secrets.h"
    echo ""
    echo "Tip: run  ./scripts/setup-mac.sh --verify  on each machine and diff the"
    echo "     output to confirm both are at parity."
}

# ============================================================================
if [[ "$MODE" == "install" ]]; then
    install_all
    doctor
    guide
else
    doctor
fi
echo ""
echo "=== Done ($MODE) ==="
