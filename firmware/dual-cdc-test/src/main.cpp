// dual-cdc-test/main.cpp — prove an ESP32-S3 can present TWO USB-CDC serial ports.
//
// Architecture §5: the receiver needs Port A (upstream sensor telemetry) and
// Port B (downstream LED commands) as two independent /dev/tty devices so LED
// writes can never block sensor reads. This sketch is the minimal proof.
//
// PASS condition: with this board alone on USB, macOS shows TWO new
//   /dev/tty.usbmodem* devices, one printing "PORT A", the other "PORT B".
// FAIL condition: only one port appears (or build fails) -> Arduino-ESP32 can't
//   do 2 CDC on this core version; fall back to ESP-IDF esp_tinyusb (see ini).

#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"

USBCDC PortA(0);   // upstream
USBCDC PortB(1);   // downstream

void setup() {
  PortA.begin();
  PortB.begin();
  USB.begin();
  delay(2000);     // let host enumerate both interfaces
}

void loop() {
  PortA.println("PORT A (upstream / sensor telemetry) — dual-CDC test");
  PortB.println("PORT B (downstream / LED commands) — dual-CDC test");
  delay(1000);
}
