// dotstar-bringup/main.cpp — standalone APA102 DotStar signs-of-life test.
//
// Proves the DotStar strip wiring + firmware in isolation (no ESP-NOW, no IMU).
// Once this animates cleanly, the same driver gets folded into osc-chain-test.
//
// Wiring (strip's INPUT end -> Feather), hardware SPI:
//   strip 5V  -> Feather BAT     strip GND -> Feather GND
//   strip DI  -> Feather MO      strip CI  -> Feather SCK
// Power from BAT (live on battery AND while charging), NEVER the 3V3 regulator
// (~500 mA peak). At 3.7-4.2 V the strip also tolerates 3.3 V data/clock better
// than at 5 V. Use the end labelled DI/CI (data/clock IN); if the strip has
// direction arrows, they point AWAY from the input end. Common ground is required.

#include <Arduino.h>
#include <Adafruit_DotStar.h>

// Generous count: extra writes are harmless, so this lights whatever is physically
// present (12 / 24 / 36 ...). Report back how many actually light.
#define NUMPIXELS 48

// No-pin constructor = hardware SPI on the board's default SPI peripheral, which on
// the ESP32-S3 Feather maps to MO (data) + SCK (clock). DOTSTAR_BRG is the usual
// Adafruit APA102 colour order; if red/blue look swapped, try DOTSTAR_BGR / DOTSTAR_GBR.
Adafruit_DotStar strip(NUMPIXELS, DOTSTAR_BGR);  // BGR on this strip: green confirmed on HW, blue unchanged, red inferred from the R/G swap

static uint32_t wheel(uint8_t pos) {  // 0-255 -> rainbow
  pos = 255 - pos;
  if (pos < 85)  return strip.Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return strip.Color(0, pos * 3, 255 - pos * 3); }
  pos -= 170;    return strip.Color(pos * 3, 255 - pos * 3, 0);
}

void setup() {
  Serial.begin(115200);
  delay(800);
  strip.begin();
  strip.setBrightness(32);   // low: safe on USB/LiPo, easy on the eyes
  strip.clear();
  strip.show();
  Serial.println("DotStar bring-up running. If you see motion, wiring + firmware are good.");
  Serial.printf("Declared %d pixels, hardware SPI (MO=data, SCK=clock).\n", NUMPIXELS);
}

static uint16_t frame = 0;

void loop() {
  for (int i = 0; i < NUMPIXELS; i++)
    strip.setPixelColor(i, wheel((uint8_t)((i * 256 / NUMPIXELS + frame) & 0xFF)));
  // a bright white dot marching along, so even a single working pixel is obvious
  strip.setPixelColor(frame % NUMPIXELS, strip.Color(255, 255, 255));
  strip.show();
  frame++;
  delay(50);
}
