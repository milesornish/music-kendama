// two-receiver-test/main.cpp — two-receiver topology, now with TWO instruments.
//
// ONE binary for every board; each self-assigns its role by its own MAC at boot:
//   INSTRUMENT (B9:70 board#1, 0D:80 board#4): read IMU, send sensor -> Receiver-UP;
//                                              also Rx downstream cmds.
//   RX_UP      (0D:20 board#2): Rx sensor from BOTH instruments -> CDC (Port A), per-sender stats.
//   RX_DOWN    (0C:E0 board#3): read a line from CDC (Port B) -> ESP-NOW -> instrument #1.
//
// Validates §2 "parallel clients": two instruments streaming to one receiver at once.
// (Board #4 sends zeros if no IMU is wired — still exercises 2-sender -> 1-receiver demux.)

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_ISM330DHCX.h>

static const uint8_t ESPNOW_CHANNEL = 1;

static uint8_t INSTRUMENT_MAC[6]   = {0xB4, 0x3A, 0x45, 0x33, 0xB9, 0x70}; // board #1
static uint8_t INSTRUMENT_MAC_2[6] = {0xB4, 0x3A, 0x45, 0x34, 0x0D, 0x80}; // board #4
static uint8_t RX_UP_MAC[6]        = {0xB4, 0x3A, 0x45, 0x34, 0x0D, 0x20}; // board #2
static uint8_t RX_DOWN_MAC[6]      = {0xB4, 0x3A, 0x45, 0x34, 0x0C, 0xE0}; // board #3

enum Role { ROLE_INSTRUMENT, ROLE_RX_UP, ROLE_RX_DOWN, ROLE_UNKNOWN };
static Role role = ROLE_UNKNOWN;

static Adafruit_ISM330DHCX imu;
static bool imu_ok = false;

typedef struct __attribute__((packed)) {
  uint32_t seq;
  float    ax, ay, az, gx, gy, gz;
} sensor_packet_t;

typedef struct __attribute__((packed)) {
  uint32_t cmd_seq;
  char     msg[28];
} cmd_packet_t;

static volatile uint32_t tx_ok = 0, tx_fail = 0;

static void macStr(const uint8_t *m, char *out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void onSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) tx_ok++; else tx_fail++;
}

// RX_UP: per-sender sequence/loss tracking so two instruments report independently.
struct SenderStat { uint8_t mac[6]; bool used; uint32_t last_seq, count, missed; };
static SenderStat senders[4] = {};
static SenderStat *getSender(const uint8_t *mac) {
  for (int i = 0; i < 4; i++) if (senders[i].used && memcmp(senders[i].mac, mac, 6) == 0) return &senders[i];
  for (int i = 0; i < 4; i++) if (!senders[i].used) { memcpy(senders[i].mac, mac, 6); senders[i].used = true; return &senders[i]; }
  return nullptr;
}
static void onRecvSensor(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(sensor_packet_t)) return;
  sensor_packet_t p; memcpy(&p, data, sizeof(p));
  SenderStat *s = getSender(mac);
  if (s) {
    if (s->count > 0 && p.seq > s->last_seq + 1) s->missed += (p.seq - s->last_seq - 1);
    s->last_seq = p.seq; s->count++;
  }
  char ms[18]; macStr(mac, ms);
  Serial.printf("[PortA UP] %s seq=%-6u a=[%6.2f %6.2f %6.2f]  (rx=%u missed=%u)\n",
                ms, p.seq, p.ax, p.ay, p.az, s ? s->count : 0, s ? s->missed : 0);
}

static void onRecvCmd(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(cmd_packet_t)) return;
  cmd_packet_t c; memcpy(&c, data, sizeof(c));
  c.msg[sizeof(c.msg) - 1] = 0;
  char ms[18]; macStr(mac, ms);
  Serial.printf("[DOWNSTREAM rx] cmd_seq=%u msg='%s' from=%s\n", c.cmd_seq, c.msg, ms);
}

static bool addPeer(uint8_t *mac) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mymac[6]; WiFi.macAddress(mymac);
  char me[18]; macStr(mymac, me);

  if (memcmp(mymac, INSTRUMENT_MAC, 6) == 0 || memcmp(mymac, INSTRUMENT_MAC_2, 6) == 0) role = ROLE_INSTRUMENT;
  else if (memcmp(mymac, RX_UP_MAC, 6) == 0)   role = ROLE_RX_UP;
  else if (memcmp(mymac, RX_DOWN_MAC, 6) == 0) role = ROLE_RX_DOWN;

  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init FAILED"); return; }

  switch (role) {
    case ROLE_RX_UP:
      esp_now_register_recv_cb(onRecvSensor);
      Serial.printf("=== RECEIVER-UP === MAC %s (Port A: upstream from both instruments)\n", me);
      break;
    case ROLE_INSTRUMENT:
      imu_ok = imu.begin_I2C();
      esp_now_register_send_cb(onSent);
      esp_now_register_recv_cb(onRecvCmd);
      addPeer(RX_UP_MAC);
      Serial.printf("=== INSTRUMENT === MAC %s, IMU %s -> sensor to Receiver-UP; listening for cmds\n",
                    me, imu_ok ? "OK" : "MISSING (zeros)");
      break;
    case ROLE_RX_DOWN:
      esp_now_register_send_cb(onSent);
      addPeer(INSTRUMENT_MAC);
      Serial.printf("=== RECEIVER-DOWN === MAC %s (Port B: type a line -> relays to instrument #1)\n", me);
      break;
    default:
      Serial.printf("=== UNKNOWN ROLE === MAC %s not in the role table!\n", me);
      break;
  }
}

static uint32_t seq = 0;
static uint32_t cmd_seq = 0;
static String buf;

void loop() {
  switch (role) {
    case ROLE_INSTRUMENT: {
      sensor_packet_t p; p.seq = seq++;
      p.ax = p.ay = p.az = p.gx = p.gy = p.gz = 0.0f;
      if (imu_ok) {
        sensors_event_t a, g, t; imu.getEvent(&a, &g, &t);
        p.ax = a.acceleration.x; p.ay = a.acceleration.y; p.az = a.acceleration.z;
        p.gx = g.gyro.x;         p.gy = g.gyro.y;         p.gz = g.gyro.z;
      }
      esp_now_send(RX_UP_MAC, (uint8_t *)&p, sizeof(p));
      delay(100); // 10 Hz
      break;
    }
    case ROLE_RX_DOWN: {
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
          if (buf.length() > 0) {
            cmd_packet_t pkt; pkt.cmd_seq = cmd_seq++;
            memset(pkt.msg, 0, sizeof(pkt.msg));
            strncpy(pkt.msg, buf.c_str(), sizeof(pkt.msg) - 1);
            esp_now_send(INSTRUMENT_MAC, (uint8_t *)&pkt, sizeof(pkt));
            Serial.printf("[PortB DOWN] relayed seq=%u msg='%s' (acked=%u fail=%u)\n",
                          pkt.cmd_seq, pkt.msg, tx_ok, tx_fail);
            buf = "";
          }
        } else {
          buf += c;
        }
      }
      delay(10);
      break;
    }
    default:
      delay(200);
      break;
  }
}
