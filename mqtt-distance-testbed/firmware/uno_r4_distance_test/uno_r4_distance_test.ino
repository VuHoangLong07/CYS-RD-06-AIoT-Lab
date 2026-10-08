// Standalone test: UNO R4 WiFi Qwiic socket -> bundled cable -> ABX00102.
#include <Arduino_Modulino.h>
#include <Wire.h>
#include <math.h>
ModulinoDistance distanceSensor;
bool sensorReady = false;

void setup() {
  Serial.begin(115200);
  unsigned long started = millis();
  while (!Serial && millis() - started < 2000) delay(10);
  Modulino.begin();  // Selects Wire1 for UNO R4 WiFi's Qwiic socket.
  sensorReady = distanceSensor.begin();
  Serial.println(sensorReady ? "Distance sensor ready; values in mm." :
    "SENSOR_NOT_FOUND: unplug USB, check Qwiic cable, reconnect and reset.");
}

void loop() {
  delay(500);
  if (!sensorReady) return;
  Wire1.beginTransmission(0x29);
  if (Wire1.endTransmission() != 0 || !distanceSensor.available()) {
    Serial.println("NO_VALID_RANGE: check target, cable and distance.");
    return;
  }
  float mm = distanceSensor.get();
  if (!isfinite(mm) || mm < 0 || mm > 1200) {
    Serial.println("NO_VALID_RANGE: outside 0-1200 mm testbed range.");
    return;
  }
  Serial.print("Distance mm: "); Serial.println(mm, 0);
}
