// bringup/main.cpp — ESP32-S3 Feather sanity check + MAC capture.
//
// Purpose:
//   1. Verify this machine can build, flash, and monitor an S3 over native USB.
//   2. Print the board's STA MAC address — this is the MAC ESP-NOW uses for peer
//      registration, so record it into firmware/{instrument,receiver}/include/secrets.h.
//
// Usage:  pio run -d firmware/bringup -t upload && pio device monitor -d firmware/bringup
//   Flash each S3 board in turn and write down the MAC each prints.
//   (Label the boards! e.g. tama / ken / receiver.)

#include <Arduino.h>
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  // Native USB-CDC enumerates a moment after boot; give the host time to attach.
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
    delay(10);
  }
  // ESP-NOW transmits from the STA interface, so the STA MAC is the one to peer on.
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.println("=== Music Kendama — S3 bringup ===");
}

void loop() {
  Serial.print("STA MAC (use this in secrets.h): ");
  Serial.println(WiFi.macAddress());
  delay(1000);
}
