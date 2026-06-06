// battery-check — read the onboard MAX17048 fuel gauge: cell voltage + state-of-charge.
// No radio / no LEDs, so this also draws the least, letting the LiPo top up fastest.
#include <Arduino.h>
#include <Adafruit_MAX1704X.h>

Adafruit_MAX17048 fg;
bool ok = false;

void setup() {
  Serial.begin(115200);
  delay(1500);
#ifdef NEOPIXEL_I2C_POWER
  pinMode(NEOPIXEL_I2C_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_I2C_POWER, HIGH);   // power the onboard I2C rail
#endif
  delay(100);
  ok = fg.begin();
  Serial.println(ok ? "BATT: MAX17048 ready" : "BATT: MAX17048 NOT FOUND");
}

void loop() {
  if (ok) {
    Serial.printf("BATT %.3f V  %.1f %%  (rate %.2f %%/hr)\n",
                  fg.cellVoltage(), fg.cellPercent(), fg.chargeRate());
  } else {
    Serial.println("BATT: no fuel gauge");
  }
  delay(1000);
}
