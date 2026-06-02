// espnow-latency/main.cpp — measure real ESP-NOW round-trip latency on hardware.
//
//   PINGER: stamps a packet with esp_timer (µs), sends to ECHO, waits for the bounce,
//           computes RTT = now - t0. One-way ≈ RTT/2. Reports min/avg/p95/max + loss
//           every BATCH samples over USB serial.
//   ECHO:   on receive, immediately sends the identical payload back to PINGER.
//
// Self-roling by MAC (same scheme as osc-chain-test). Every board prints its own MAC
// + resolved role at boot, so an unrecognised board announces itself for the table.
// No IMU, no LEDs — only two boards + the PINGER's USB cable are needed.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <stdlib.h>
#include "protocol.h"

// Known boards (efuse MACs validated 2026-05-28). PINGER/ECHO are assigned to two of
// them; if your in-hand boards differ, the boot banner prints each board's MAC so we
// can repoint these two lines and reflash.
static uint8_t PINGER_MAC[6] = {0xB4, 0x3A, 0x45, 0x33, 0xB9, 0x70}; // board #1
static uint8_t ECHO_MAC[6]   = {0xB4, 0x3A, 0x45, 0x34, 0x0D, 0x20}; // board #2

enum Role { ROLE_PINGER, ROLE_ECHO, ROLE_UNASSIGNED };
static Role role = ROLE_UNASSIGNED;

typedef struct __attribute__((packed)) { uint32_t seq; uint32_t t0_us; } ping_t;

#define BATCH        500     // samples per reported batch
#define TIMEOUT_MS    50     // a ping with no echo within this is counted as lost

static bool eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static bool addPeer(const uint8_t *mac) {
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = KENDAMA_ESPNOW_CHANNEL;
  p.encrypt = false;
  return esp_now_add_peer(&p) == ESP_OK;
}

// --- PINGER state (written in the recv callback, read in loop) ---
static volatile uint32_t cur_seq    = 0;
static volatile bool     got_echo   = false;
static volatile uint32_t echo_rtt   = 0;

// ECHO: bounce the exact bytes straight back to the pinger.
static void onEcho(const uint8_t *mac, const uint8_t *data, int len) {
  esp_now_send(PINGER_MAC, data, len);
}

// PINGER: a returning packet — if it matches the outstanding seq, record the RTT.
static void onReturn(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(ping_t)) return;
  ping_t p; memcpy(&p, data, sizeof(p));
  if (p.seq == cur_seq && !got_echo) {
    echo_rtt = (uint32_t)esp_timer_get_time() - p.t0_us;
    got_echo = true;
  }
}

static int cmp_u32(const void *a, const void *b) {
  uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
  return (x > y) - (x < y);
}

static uint32_t samples[BATCH];
static uint32_t count = 0, lost = 0;

static void report() {
  qsort(samples, count, sizeof(uint32_t), cmp_u32);
  uint64_t sum = 0;
  for (uint32_t i = 0; i < count; i++) sum += samples[i];
  uint32_t avg = count ? (uint32_t)(sum / count) : 0;
  uint32_t p95 = count ? samples[(uint32_t)(count * 0.95)] : 0;
  uint32_t mn = count ? samples[0] : 0;
  uint32_t mx = count ? samples[count - 1] : 0;
  Serial.printf("RTT us  min=%u  avg=%u  p95=%u  max=%u   | one-way us  avg=%u  p95=%u"
                "   | samples=%u lost=%u (%.1f%%)\n",
                mn, avg, p95, mx, avg / 2, p95 / 2, count, lost,
                (count + lost) ? (100.0 * lost / (count + lost)) : 0.0);
  count = 0; lost = 0;
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(KENDAMA_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  if      (eq(mac, PINGER_MAC)) role = ROLE_PINGER;
  else if (eq(mac, ECHO_MAC))   role = ROLE_ECHO;

  Serial.printf("MAC %02X:%02X:%02X:%02X:%02X:%02X  role=%s\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                role == ROLE_PINGER ? "PINGER" : role == ROLE_ECHO ? "ECHO" : "UNASSIGNED (tell host)");

  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init FAILED"); return; }

  if (role == ROLE_PINGER) {
    addPeer(ECHO_MAC);
    esp_now_register_recv_cb(onReturn);
    Serial.println("PINGER ready — measuring...");
  } else if (role == ROLE_ECHO) {
    addPeer(PINGER_MAC);
    esp_now_register_recv_cb(onEcho);
    Serial.println("ECHO ready — bouncing pings.");
  }
}

void loop() {
  if (role != ROLE_PINGER) { delay(200); return; }

  ping_t p;
  p.seq   = cur_seq;
  p.t0_us = (uint32_t)esp_timer_get_time();
  got_echo = false;
  esp_now_send(ECHO_MAC, (uint8_t *)&p, sizeof(p));

  uint32_t start = millis();
  while (!got_echo && (millis() - start) < TIMEOUT_MS) { delay(1); }

  if (got_echo) { if (count < BATCH) samples[count++] = echo_rtt; }
  else          { lost++; }

  cur_seq++;
  if (count >= BATCH) report();
}
