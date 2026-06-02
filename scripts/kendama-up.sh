#!/usr/bin/env bash
# kendama-up.sh — re-establish the live signal chain after a reboot / laptop sleep.
#
# Laptop sleep kills the bridge process and power-cycles the boards (resetting their
# stream rate), and USB re-enumeration can shuffle port names. This script is
# port-agnostic: it identifies the receivers by their efuse MAC, starts the C bridge,
# sets the instrument stream rate, and verifies OSC is flowing.
#
#   usage: ./scripts/kendama-up.sh [rate_hz]      (default 100)
#
# M4L binds UDP :9000 to receive; this script reads the :9500 debug tap so it never
# fights M4L for the port.
set -uo pipefail
HZ="${1:-100}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIO_DIR="$ROOT/firmware/osc-chain-test"
BRIDGE="$ROOT/bridge/music-kendama-bridge"

RX_UP_MAC="b4:3a:45:34:0d:20"    # board #2
RX_DOWN_MAC="b4:3a:45:34:0c:e0"  # board #3

echo "kendama-up: stopping any running bridge..."
pkill -f music-kendama-bridge 2>/dev/null || true
sleep 1

[ -x "$BRIDGE" ] || { echo "  ! bridge binary missing — run 'make' in bridge/ first"; exit 1; }

echo "kendama-up: identifying receivers by MAC (port-agnostic)..."
RXUP=""; RXDOWN=""
for p in /dev/cu.usbmodem*; do
  [ -e "$p" ] || continue
  mac=$(cd "$PIO_DIR" && pio pkg exec -- esptool.py --chip esp32s3 --port "$p" read_mac 2>/dev/null \
        | grep -iE "^MAC:" | head -1 | awk '{print tolower($2)}')
  case "$mac" in
    "$RX_UP_MAC")   RXUP="$p";   echo "  $p = RX_UP   (#2)";;
    "$RX_DOWN_MAC") RXDOWN="$p"; echo "  $p = RX_DOWN (#3)";;
    *)              echo "  $p = $mac (instrument / other)";;
  esac
done

[ -n "$RXUP" ] || { echo "  ! RX_UP (#2) not found on USB — is it plugged in?"; exit 1; }
if [ -z "$RXDOWN" ]; then
  echo "  ! RX_DOWN (#3) not found — starting UP-only (no LED / rate down-path)"
  "$BRIDGE" "$RXUP" > /tmp/bridge.log 2>&1 &
else
  "$BRIDGE" "$RXUP" "$RXDOWN" > /tmp/bridge.log 2>&1 &
fi
echo "kendama-up: bridge pid $! (Port A=$RXUP  Port B=${RXDOWN:-none})"
sleep 2

python3 - "$HZ" "${RXDOWN:-}" <<'PY'
import socket, struct, time, sys
hz = int(sys.argv[1]); have_down = bool(sys.argv[2])
def osc(addr, ints):
    s = addr.encode()+b'\x00'; s += b'\x00'*((4-len(s)%4)%4)
    t = b','+b'i'*len(ints)+b'\x00'; t += b'\x00'*((4-len(t)%4)%4)
    return s+t+struct.pack('>'+'i'*len(ints), *ints)
if have_down:
    u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    iv = max(1, round(1e6/hz))
    for _ in range(5):
        u.sendto(osc('/kendama/rate', [iv]), ('127.0.0.1', 9001)); time.sleep(0.1)
    u.close(); print(f"kendama-up: set stream rate -> {hz}/s")
r = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
r.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
r.bind(('127.0.0.1', 9500)); r.settimeout(0.5)
seen = {}; t = time.time()
while time.time()-t < 3:
    try:
        d,_ = r.recvfrom(2048); a = d.split(b'\x00',1)[0].decode('ascii','replace'); seen[a] = seen.get(a,0)+1
    except Exception: pass
r.close()
print("kendama-up: OSC live ->", {k: round(v/3) for k,v in seen.items()}, "/s" if seen else "(NONE — check instruments powered)")
PY
echo "kendama-up: ready. (Live/M4L binds :9000; the device drives sound + LEDs from here.)"
