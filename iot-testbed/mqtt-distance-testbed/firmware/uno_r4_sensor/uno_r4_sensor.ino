// UNO R4 WiFi + Modulino Distance ABX00102, bundled cable in the Qwiic socket.
// Libraries: Arduino_Modulino (install its dependencies), ArduinoMqttClient.
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <Arduino_Modulino.h>
#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lab_config.h"

const char DEVICE_ID[] = "room1-sensor-01";
const char FIRMWARE_VERSION[] = "distance-sensor-1.0.0";
const char DISTANCE_TOPIC[] = "lab/room1/distance";
const char STATUS_TOPIC[] = "lab/room1/sensor/status";
const unsigned long SAMPLE_INTERVAL_MS = 5000;
const unsigned long WIFI_RETRY_MS = 15000;
const unsigned long MQTT_RETRY_MS = 5000;

ModulinoDistance distanceSensor;
WiFiClient network;
MqttClient mqtt(network);
bool sensorReady = false;
bool wifiIdentityPrinted = false;
unsigned long lastSample = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long seq = 0;

bool sendText(const char* topic, const char* payload, bool retained) {
  if (!mqtt.beginMessage(topic, (unsigned long)strlen(payload), retained, 1)) return false;
  mqtt.print(payload);
  return mqtt.endMessage() == 1;
}

void printIdentity() {
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char text[18];
  snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("DEVICE="); Serial.println(DEVICE_ID);
  Serial.print("FIRMWARE="); Serial.println(FIRMWARE_VERSION);
  Serial.print("BUILD="); Serial.println(__DATE__ " " __TIME__);
  Serial.print("WIFI_FW="); Serial.println(WiFi.firmwareVersion());
  Serial.print("MAC="); Serial.println(text);
  Serial.print("IP="); Serial.println(WiFi.localIP());
}

bool ensureConnected() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    wifiIdentityPrinted = false;
    if (mqtt.connected()) mqtt.stop();
    if (now - lastWifiAttempt >= WIFI_RETRY_MS) {
      lastWifiAttempt = now;
      Serial.println("Connecting to lab Wi-Fi...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return false;
  }
  if (!wifiIdentityPrinted) { printIdentity(); wifiIdentityPrinted = true; }
  if (mqtt.connected()) return true;
  if (now - lastMqttAttempt < MQTT_RETRY_MS) return false;
  lastMqttAttempt = now;
  if (!mqtt.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.print("MQTT connect error="); Serial.println(mqtt.connectError());
    return false;
  }
  sendText(STATUS_TOPIC, "ONLINE", true);
  Serial.println("MQTT ready; distance readings every 5 seconds, in mm.");
  return true;
}

void setup() {
  Serial.begin(115200);
  unsigned long started = millis();
  while (!Serial && millis() - started < 2000) delay(10);
  // On UNO R4 WiFi, this selects the Qwiic bus, Wire1, automatically.
  Modulino.begin();
  sensorReady = distanceSensor.begin();
  if (!sensorReady) {
    Serial.println("SENSOR_NOT_FOUND: unplug USB, check Qwiic cable, reconnect and reset.");
    return;
  }
  mqtt.setId(DEVICE_ID);
  mqtt.setCleanSession(true);
  mqtt.setKeepAliveInterval(60000);
  mqtt.setConnectionTimeout(3000);
  if (MQTT_USERNAME[0] != '\0') mqtt.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);
  mqtt.beginWill(STATUS_TOPIC, (unsigned short)7, true, 1);
  mqtt.print("OFFLINE");
  mqtt.endWill();
  lastWifiAttempt = millis();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastSample = millis();
}

void loop() {
  if (!sensorReady) { delay(100); return; }
  if (!ensureConnected()) { delay(10); return; }
  mqtt.poll();
  unsigned long now = millis();
  if (now - lastSample < SAMPLE_INTERVAL_MS) { delay(10); return; }
  lastSample = now;
  // Probe the Qwiic bus before using the library's latest valid range result.
  Wire1.beginTransmission(0x29);
  if (Wire1.endTransmission() != 0 || !distanceSensor.available()) {
    Serial.println("SENSOR_READ_FAILED: check cable, target and range; reading skipped.");
    return;
  }
  float mm = distanceSensor.get();
  if (!isfinite(mm) || mm < 0 || mm > 1200) {
    Serial.println("INVALID_RANGE: outside the 0-1200 mm testbed range; reading skipped.");
    return;
  }
  char payload[320];
  int length = snprintf(payload, sizeof(payload),
    "{\"device\":\"%s\",\"firmware\":\"%s\",\"seq\":%lu,\"uptime_ms\":%lu,"
    "\"distance_mm\":%.0f,\"unit\":\"mm\",\"valid\":true}",
    DEVICE_ID, FIRMWARE_VERSION, ++seq, now, mm);
  if (length < 0 || (size_t)length >= sizeof(payload)) {
    Serial.println("Payload too large; reading skipped.");
    return;
  }
  if (sendText(DISTANCE_TOPIC, payload, false)) {
    Serial.print("SENT "); Serial.print(DISTANCE_TOPIC); Serial.print(" "); Serial.println(payload);
  } else {
    Serial.println("PUBLISH_FAILED: reading not acknowledged; check broker.");
  }
}
