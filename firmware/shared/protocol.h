// protocol.h — shared wire conventions for the Music Kendama wireless core.
#ifndef KENDAMA_PROTOCOL_H
#define KENDAMA_PROTOCOL_H

#include <stdint.h>

// All ESP-NOW nodes use a fixed 2.4 GHz channel.
#define KENDAMA_ESPNOW_CHANNEL 1

// Compact sensor payload carried over ESP-NOW (instrument -> receiver). The receiver
// converts this to OSC and SLIP-frames it onto USB Port A for the laptop. Keeping the
// over-the-air payload as a packed struct (not OSC) keeps it small; bench-tested
// 2026-05-28 with a single instrument, accel/gyro only. Quaternion fields are populated once the LSM6DSV16X SFLP is
// wired (ISM330DHCX stand-in leaves them zero and fills accel/gyro only).
typedef struct __attribute__((packed)) {
  uint32_t seq;
  float    qw, qx, qy, qz;   // orientation quaternion (SFLP); 0 if unavailable
  float    ax, ay, az;       // accel, m/s^2
  float    gx, gy, gz;       // gyro, rad/s
} kendama_sensor_t;

// OSC address namespace (architecture §11). Device role selects the prefix.
#define OSC_TAMA_ORIENTATION "/kendama/tama/orientation"  // ,ffff (qw qx qy qz)
#define OSC_TAMA_ACCEL       "/kendama/tama/accel"        // ,fff
#define OSC_TAMA_GYRO        "/kendama/tama/gyro"         // ,fff
#define OSC_KEN_ORIENTATION  "/kendama/ken/orientation"
#define OSC_KEN_ACCEL        "/kendama/ken/accel"
#define OSC_KEN_GYRO         "/kendama/ken/gyro"

// Downstream LED commands (laptop -> Receiver-DOWN -> instrument). ,iii = R G B (0-255)
#define OSC_TAMA_LED_RGB     "/kendama/tama/led/rgb"
#define OSC_KEN_LED_RGB      "/kendama/ken/led/rgb"

// APA102 DotStar ring sizes per instrument (144 LED/m geometry). Validate against the
// physical cut: ken base-cup ring ~24 (Ø~53 mm), tama ring 36 (Ø80 mm).
#define KENDAMA_KEN_LED_COUNT   24
#define KENDAMA_TAMA_LED_COUNT  36

// Test instrumentation: set an instrument's send interval in microseconds. ,i
#define OSC_SET_RATE "/kendama/rate"

// Test: set an instrument's uplink PHY rate. ,i  (1 -> ~1 Mbps robust, 54 -> 54 Mbps fast).
// The downlink (commands) stays at the default rate, so this is always reachable.
#define OSC_SET_PHY "/kendama/phy"

#endif // KENDAMA_PROTOCOL_H
