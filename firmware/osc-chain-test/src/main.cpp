// osc-chain-test/main.cpp — full bidirectional OSC chain on hardware.
//
//   UP:   instrument IMU -> ESP-NOW -> Receiver-UP -> OSC+SLIP -> USB -> bridge -> UDP :9000
//   DOWN: UDP :9001 -> bridge -> SLIP -> Receiver-DOWN -> ESP-NOW -> instrument -> NeoPixel
//
// Self-roling by MAC. Receiver-UP/DOWN each print a one-time ASCII marker so the host can
// find their ports, then Receiver-UP goes pure binary SLIP out; Receiver-DOWN reads binary
// SLIP in. Instruments light their onboard NeoPixel on a matching OSC /led/rgb command.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_ISM330DHCX.h>
#include <Adafruit_DotStar.h>
#include "protocol.h"
#include "osc.h"
#include "slip.h"

static uint8_t INSTRUMENT_KEN[6]  = {0xB4, 0x3A, 0x45, 0x33, 0xB9, 0x70}; // board #1 (ken assembly)
static uint8_t INSTRUMENT_TAMA[6] = {0xB4, 0x3A, 0x45, 0x34, 0x0D, 0x80}; // board #4 (tama/dama)
static uint8_t RX_UP_MAC[6]       = {0xB4, 0x3A, 0x45, 0x34, 0x0D, 0x20}; // board #2
static uint8_t RX_DOWN_MAC[6]     = {0xB4, 0x3A, 0x45, 0x34, 0x0C, 0xE0}; // board #3
static uint8_t BCAST_MAC[6]       = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t *up_target = RX_UP_MAC;   // instrument uplink dest (broadcast under ESPNOW_BROADCAST)

enum Role { ROLE_INSTRUMENT, ROLE_RX_UP, ROLE_RX_DOWN, ROLE_UNKNOWN };
static Role role = ROLE_UNKNOWN;
static bool is_ken = false;
static const char *my_led_addr = OSC_TAMA_LED_RGB;

static Adafruit_ISM330DHCX imu;
static bool imu_ok = false;

static volatile uint32_t rx_count = 0;   // RX_COUNT_MODE: ESP-NOW receptions since last report

// External APA102 DotStar strip on hardware SPI (data -> MO, clock -> SCK). Constructed at
// the max ring size; the actually-lit count is set per role (ken vs tama) in setup().
#define LED_MAX 48
static Adafruit_DotStar strip(LED_MAX, DOTSTAR_BGR);  // BGR on this strip: green confirmed on HW, blue unchanged, red inferred from the R/G swap
static uint16_t led_count = LED_MAX;

// Instrument send pacing (us). Default 10/s; retunable at runtime via OSC_SET_RATE (test).
static uint32_t send_interval_us = 100000;
static uint32_t last_send_us = 0;

#ifndef BATCH_N
#define BATCH_N 1            // samples packed per ESP-NOW frame (5*48=240B <= 250B cap)
#endif

// Test: refresh the IMU off the send hot-path (~50 Hz) so the radio, not the I2C read,
// is the throughput limiter. Sends carry the most recent cached sample.
static uint32_t imu_interval_us = 20000;
static uint32_t last_imu_us = 0;
static float cax, cay, caz, cgx, cgy, cgz;

static bool eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static bool addPeer(uint8_t *mac) {
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = KENDAMA_ESPNOW_CHANNEL;
  p.encrypt = false;
  return esp_now_add_peer(&p) == ESP_OK;
}

// INSTRUMENT: parse an incoming OSC LED command; light the onboard pixel if addressed to me.
static void onLed(const uint8_t *mac, const uint8_t *data, int len) {
  char addr[48];
  osc_reader_t r;
  if (!osc_parse(data, (size_t)len, addr, sizeof(addr), &r)) return;
  if (strcmp(addr, OSC_SET_RATE) == 0) {           // test: retune this instrument's send interval (us)
    int32_t iv;
    if (osc_read_int(&r, &iv) && iv > 0) send_interval_us = (uint32_t)iv;
    return;
  }
  if (strcmp(addr, OSC_SET_PHY) == 0) {            // test: switch uplink PHY rate (1 or 54 Mbps)
    int32_t mbps;
    if (osc_read_int(&r, &mbps)) {
      esp_wifi_config_espnow_rate(WIFI_IF_STA, mbps >= 54 ? WIFI_PHY_RATE_54M : WIFI_PHY_RATE_1M_L);
    }
    return;
  }
  if (strcmp(addr, my_led_addr) != 0) return;
  int32_t R, G, B;
  if (osc_read_int(&r, &R) && osc_read_int(&r, &G) && osc_read_int(&r, &B)) {
    uint32_t c = strip.Color((uint8_t)R, (uint8_t)G, (uint8_t)B);
    for (uint16_t i = 0; i < led_count; i++) strip.setPixelColor(i, c);
    strip.show();
    Serial.printf("LED rgb=(%d,%d,%d) -> %d px\n", (int)R, (int)G, (int)B, led_count);
  }
}

// RX_UP: wrap a received sensor packet as OSC + SLIP, write to serial.
static void onSensor(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(kendama_sensor_t)) return;
#ifdef RX_COUNT_MODE
  rx_count += (uint32_t)(len / (int)sizeof(kendama_sensor_t));  // count samples (handles batched frames)
  return;
#endif
  kendama_sensor_t s;
  memcpy(&s, data, sizeof(s));
  const char *addr = eq(mac, INSTRUMENT_KEN) ? OSC_KEN_ACCEL : OSC_TAMA_ACCEL;
  uint8_t obuf[96];
  osc_t m; osc_init(&m, obuf, sizeof(obuf));
  // seq prepended so the host can measure per-instrument loss over a soak (gaps in seq).
  osc_begin(&m, addr, "ifff");
  osc_add_int(&m, (int32_t)s.seq);
  osc_add_float(&m, s.ax); osc_add_float(&m, s.ay); osc_add_float(&m, s.az);
  if (!osc_ok(&m)) return;
  uint8_t sb[200];
  size_t e = slip_encode(obuf, m.len, sb, sizeof(sb));
  if (e) Serial.write(sb, e);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(KENDAMA_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  if (eq(mac, INSTRUMENT_TAMA) || eq(mac, INSTRUMENT_KEN)) {
    role = ROLE_INSTRUMENT;
    is_ken = eq(mac, INSTRUMENT_KEN);
    my_led_addr = is_ken ? OSC_KEN_LED_RGB : OSC_TAMA_LED_RGB;
  } else if (eq(mac, RX_UP_MAC))   role = ROLE_RX_UP;
  else if (eq(mac, RX_DOWN_MAC))   role = ROLE_RX_DOWN;

  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init FAILED"); return; }
#ifdef ESPNOW_PHY_RATE
  esp_wifi_config_espnow_rate(WIFI_IF_STA, ESPNOW_PHY_RATE);   // raise ESP-NOW TX PHY rate (test)
#endif

  if (role == ROLE_RX_UP) {
    esp_now_register_recv_cb(onSensor);
    Serial.println("KENDAMA_RXUP_PORT");
    Serial.flush();
  } else if (role == ROLE_RX_DOWN) {
    addPeer(INSTRUMENT_TAMA);
    addPeer(INSTRUMENT_KEN);
    Serial.println("KENDAMA_RXDOWN_PORT");
    Serial.flush();
  } else if (role == ROLE_INSTRUMENT) {
#ifdef NEOPIXEL_I2C_POWER
    pinMode(NEOPIXEL_I2C_POWER, OUTPUT); digitalWrite(NEOPIXEL_I2C_POWER, HIGH); // powers STEMMA/IMU
#endif
    led_count = is_ken ? KENDAMA_KEN_LED_COUNT : KENDAMA_TAMA_LED_COUNT;
    strip.begin(); strip.setBrightness(40); strip.clear(); strip.show();
    imu_ok = imu.begin_I2C();
    esp_now_register_recv_cb(onLed);
#ifdef ESPNOW_BROADCAST
    addPeer(BCAST_MAC); up_target = BCAST_MAC;
#else
    addPeer(RX_UP_MAC);
#endif
    Serial.printf("INSTRUMENT %s, IMU %s\n", is_ken ? "ken" : "tama", imu_ok ? "OK" : "MISSING");
  } else {
    Serial.println("UNKNOWN ROLE");
  }
}

static uint32_t seq = 0;
static slip_decoder_t down_dec;
static uint8_t down_frame[256];
static bool down_init = false;

void loop() {
  if (role == ROLE_INSTRUMENT) {
    uint32_t now = micros();
    if (imu_ok && (now - last_imu_us >= imu_interval_us)) {   // refresh cache off the hot path
      last_imu_us = now;
      sensors_event_t a, g, t;
      imu.getEvent(&a, &g, &t);
      cax = a.acceleration.x; cay = a.acceleration.y; caz = a.acceleration.z;
      cgx = g.gyro.x;         cgy = g.gyro.y;         cgz = g.gyro.z;
    }
    if (now - last_send_us < send_interval_us) return;   // rate-gate (us)
    last_send_us = now;
    static kendama_sensor_t batch[BATCH_N];              // BATCH_N samples per frame
    for (int i = 0; i < BATCH_N; i++) {
      memset(&batch[i], 0, sizeof(batch[i]));
      batch[i].seq = seq++;
      batch[i].ax = cax; batch[i].ay = cay; batch[i].az = caz;
      batch[i].gx = cgx; batch[i].gy = cgy; batch[i].gz = cgz;
    }
    esp_now_send(up_target, (uint8_t *)batch, sizeof(batch));
  } else if (role == ROLE_RX_DOWN) {
    if (!down_init) { slip_decoder_init(&down_dec, down_frame, sizeof(down_frame)); down_init = true; }
    while (Serial.available()) {
      size_t f = slip_decode_byte(&down_dec, (uint8_t)Serial.read());
      if (f) {                                   // a complete OSC frame from the bridge
        esp_now_send(INSTRUMENT_TAMA, down_frame, f);
        esp_now_send(INSTRUMENT_KEN, down_frame, f);
      }
    }
  } else {   // ROLE_RX_UP (callback-driven) or unknown
#ifdef RX_COUNT_MODE
    static uint32_t last = 0;
    if (millis() - last >= 1000) {
      last = millis();
      uint32_t c = rx_count; rx_count = 0;
      uint8_t obuf[48]; osc_t m; osc_init(&m, obuf, sizeof(obuf));
      osc_begin(&m, "/kendama/rxstat", "i");
      osc_add_int(&m, (int32_t)c);
      if (osc_ok(&m)) { uint8_t sb[64]; size_t e = slip_encode(obuf, m.len, sb, sizeof(sb)); if (e) Serial.write(sb, e); }
    }
#else
    delay(50);
#endif
  }
}
