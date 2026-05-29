// espnow-test/main.cpp — ESP-NOW link validation (architecture §4) + IMU payload (§ sensor path).
//
// Build env selects the role:
//   sender   (board #1): reads ISM330DHCX over I2C, sends {seq, t, accel, gyro} every 100 ms.
//   receiver (board #2): prints each received packet, tracks dropped sequence numbers.
//
// Same WiFi channel on both is mandatory for ESP-NOW; we pin channel 1 explicitly.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#ifdef ROLE_SENDER
#include <Adafruit_ISM330DHCX.h>
static Adafruit_ISM330DHCX imu;
static bool imu_ok = false;
#endif

static const uint8_t ESPNOW_CHANNEL = 1;

// Board #2 (RECEIVER) MAC — the sender's unicast peer.
static uint8_t RX_MAC[6] = {0xB4, 0x3A, 0x45, 0x34, 0x0D, 0x20};

typedef struct __attribute__((packed)) {
  uint32_t seq;
  float    t_sec;
  float    ax, ay, az;   // accel, m/s^2
  float    gx, gy, gz;   // gyro,  rad/s
} packet_t;

static void macStr(const uint8_t *m, char *out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

#ifdef ROLE_RECEIVER
static uint32_t rx_count = 0, last_seq = 0, missed = 0;
static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(packet_t)) return;
  packet_t p;
  memcpy(&p, data, sizeof(p));
  if (rx_count > 0 && p.seq > last_seq + 1) missed += (p.seq - last_seq - 1);
  last_seq = p.seq;
  rx_count++;
  char ms[18];
  macStr(mac, ms);
  Serial.printf("RX seq=%-6u a=[%6.2f %6.2f %6.2f] g=[%6.2f %6.2f %6.2f] from=%s (rx=%u missed=%u)\n",
                p.seq, p.ax, p.ay, p.az, p.gx, p.gy, p.gz, ms, rx_count, missed);
}
#endif

#ifdef ROLE_SENDER
static volatile uint32_t tx_ok = 0, tx_fail = 0;
static void onSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) tx_ok++;
  else tx_fail++;
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1500);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mymac[6];
  WiFi.macAddress(mymac);
  char me[18];
  macStr(mymac, me);

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init FAILED");
    return;
  }

#ifdef ROLE_RECEIVER
  esp_now_register_recv_cb(onRecv);
  Serial.printf("=== ESP-NOW RECEIVER ready === MAC %s, channel %u\n", me, ESPNOW_CHANNEL);
#endif

#ifdef ROLE_SENDER
  imu_ok = imu.begin_I2C();   // STEMMA QT / Qwiic, default addr 0x6A
  Serial.printf("ISM330DHCX: %s\n", imu_ok ? "OK" : "NOT FOUND (sending zeros)");

  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, RX_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("add_peer FAILED");
  char ps[18];
  macStr(RX_MAC, ps);
  Serial.printf("=== ESP-NOW SENDER ready === MAC %s -> peer %s, channel %u\n",
                me, ps, ESPNOW_CHANNEL);
#endif
}

#ifdef ROLE_SENDER
static uint32_t seq = 0;
#endif

void loop() {
#ifdef ROLE_SENDER
  packet_t p;
  p.seq = seq++;
  p.t_sec = millis() / 1000.0f;
  p.ax = p.ay = p.az = p.gx = p.gy = p.gz = 0.0f;
  if (imu_ok) {
    sensors_event_t accel, gyro, temp;
    imu.getEvent(&accel, &gyro, &temp);
    p.ax = accel.acceleration.x; p.ay = accel.acceleration.y; p.az = accel.acceleration.z;
    p.gx = gyro.gyro.x;          p.gy = gyro.gyro.y;          p.gz = gyro.gyro.z;
  }
  esp_now_send(RX_MAC, (uint8_t *)&p, sizeof(p));
  if (p.seq % 10 == 0) {  // throttle sender-side log; receiver is the validation surface
    Serial.printf("TX seq=%-6u a=[%6.2f %6.2f %6.2f]  (acked=%u fail=%u)\n",
                  p.seq, p.ax, p.ay, p.az, tx_ok, tx_fail);
  }
  delay(100);   // 10 Hz
#else
  delay(200);
#endif
}
