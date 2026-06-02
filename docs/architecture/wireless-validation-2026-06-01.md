# Wireless Core — Bench Validation Log (2026-06-01)

Second hardware session. Builds on [2026-05-28](wireless-validation-2026-05-28.md): brings up the
full bidirectional OSC chain with DotStar LEDs, then characterizes the ESP-NOW transport's
throughput ceiling and scaling levers. Same honesty rule — records what was **observed (and under
what conditions)** vs. **still assumed**. Does not modify the spec.

## Setup
- Same 4× ESP32-S3 Feather (efuse MACs as 2026-05-28). Roles by MAC: #1 ken / #4 tama instruments,
  #2 RX_UP, #3 RX_DOWN.
- IMU: **ISM330DHCX** stand-in — accel/gyro only; **quaternion/orientation untested** (needs LSM6DSV16X).
- LEDs: **APA102 DotStar** (white, 144 LED/m), hardware SPI (data→MO, clock→SCK), powered from **BAT**.
- Conditions unless noted: **indoor bench, <~1 m range, channel 1, low RF congestion, single runs.**
- Firmware: `firmware/osc-chain-test/` (self-roling). Diagnostic env builds: `rxcount` (count-only RX),
  `bcast` (no-ACK uplink), `batch` (5 samples/frame), `phy` (54 Mbps TX). Host: `bridge/` + ad-hoc
  python over UDP 9000/9001.

## Observed (with conditions)

### Signal chain
1. **DotStar bring-up** — APA102 lit on hardware. Color order **DOTSTAR_BGR** (green confirmed directly;
   blue unaffected by the R/G swap; **red inferred**, not directly re-checked post-fix).
2. **Full bidirectional OSC chain, one instrument** — IMU → ESP-NOW → RX_UP → OSC/SLIP → `bridge` →
   UDP 9000 (`/kendama/ken/accel`); and laptop OSC → UDP 9001 → `bridge` → RX_DOWN → ESP-NOW →
   instrument DotStar. 24-px ken ring lit on command, colors correct.
3. **Two concurrent instruments** — ken (#1) + tama (#4) streaming to one RX_UP at ~10 Hz each, with
   **independent LED addressing** (each reacts only to its own `/kendama/<role>/led/rgb`; confirmed
   via the lit strip *and* the other board's serial; no cross-talk).

### Latency
4. **ESP-NOW round-trip** (`firmware/espnow-latency/`, ping/pong): one-way **~1.1 ms avg, ~1.7–2.0 ms p95**;
   RTT min 1.89 / avg ~2.2 / p95 ~3.5 / max ~13 ms; **0 loss / 4,000 round-trips**. One run, <1 m.

### Loss
5. **3-minute two-instrument soak at ~10 Hz** (per-instrument seq-gap, end-to-end host-observed):
   **0 / 3,530 lost (0.0000%)**, 9.80 pkt/s each, no resets. One run, <1 m. (Replaces the earlier
   eyeballed packet-balance; upstream OSC now carries `seq`.)

### Throughput ceiling & scaling levers (main dig)
IMU read moved off the send hot-path so the **radio, not the I2C read**, is the limiter. Measured
delivered/received rate. All **single runs, <1 m, clean RF**:

| Configuration | Delivered ceiling |
|---|---|
| 1 instrument · unicast · 1 sample/frame · ~1 Mbps PHY (default) | ~720 pkt/s |
| 2 instruments (aggregate), same | ~720 pkt/s — **shared, not additive** (~360 each) |
| Broadcast (no ACK) | ~895 pkt/s (~1.24×) |
| Batched, 5 samples/frame (unicast, 1 Mbps) | **~1,770 samples/s** (~2.46×; packet rate falls to ~354) |
| PHY rate → 54 Mbps (unicast, 1 sample) | **~5,000 pkt/s** (~7×) |
| **Batched (5/frame) + 54 Mbps (combined)** | **~22,250 samples/s** (~31×; ~4,450 pkt/s) |

**Mechanism (isolated, not assumed):**
- A **light-callback test** (RX_UP counts receptions only, no per-packet serial) showed the *same*
  ~720 ceiling → the limit is the **ESP-NOW air path**, **not** the receiver's OSC/SLIP/serial write
  (that has headroom). This **corrected an earlier wrong guess** that blamed the serial path.
- Two instruments share the ~720 (contend for one receiver/channel) → not per-link.
- Per-frame cost ≈ **~1 ms fixed overhead + ~7 µs/byte (≈1 Mbps payload airtime)**. Batching amortizes
  the fixed part (→ ~2.5×, not 5×); higher PHY rate cuts per-byte airtime (→ ~7× at 54 Mbps). Levers
  **stack ~multiplicatively** — combined batched + 54 Mbps reached **~22,250 samples/s (~31× baseline)**.

### Range vs PHY rate
Mobile instrument (board #1, battery) at a fixed 100 Hz; uplink loss at 1 Mbps vs 54 Mbps by distance
(seq-gap, host-observed; downlink commands stay at 1 Mbps so the unit is reachable even when its
54 Mbps uplink drops — confirmed working):

| Distance | 1 Mbps loss | 54 Mbps loss |
|---|---|---|
| <1 m (next to RX) | 0.00% | 0.00% |
| ~2 m (across a small room) | **0.00%** | **78%** |
| ~4 m (next room, through a wall) | **0.00%** | **99%** |

**1 Mbps is robust** — 0% loss through a wall at ~4 m (the limit of the test space; its true range
limit was not reached). **54 Mbps collapses past ~1 m.** A clean 1 Mbps run at 4 m also rules out
battery brownout on the mobile unit.

**Takeaway (corrected by the range test):** real instrument need is ~100–200 Hz; the **robust 1 Mbps
default** (~720/s, lossless to ~4 m through a wall) is 3–5× over that. **Batching is the range-safe
throughput lever** (same PHY rate, bigger frames). **Raising the PHY rate is NOT usable** at playing
distance — its big throughput gains evaporate past ~1 m.

## NOT yet validated (the asterisks)
- **Range / RF robustness** — the PHY-rate tradeoff is now *measured* (see above: 54 Mbps dead past
  ~1 m; 1 Mbps clean to ~4 m through a wall). Still untested: **1 Mbps's actual max range** (>4 m, not
  reached in the small test space), **RF congestion / crowded-2.4 GHz / stage conditions**, and
  multi-instrument behaviour at distance.
- **Orientation / quaternions** — gesture-critical; ISM330DHCX leaves them zero.
- **Physical LED ring fit** (24/36 are design targets; strip uncut, drove 24 of 48); **measured current
  draw** (estimated only — INA219 unused); battery runtime; thermal / >3-min soak.
- **Loss at high rate** — measured *delivered* ceilings, did not separately soak loss above ~10 Hz.
- All throughput numbers are **single runs, <1 m, one room's RF**.
- Production firmware (`firmware/instrument`, `firmware/receiver`), final LSM6DSV16X, M4L → Ableton.
