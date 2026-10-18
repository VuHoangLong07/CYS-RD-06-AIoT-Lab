// Arduino UNO R4 WiFi: use the built-in L LED first (no extra parts).
// Optional external LED: change LED_PIN to 7; D7 -> 1 kOhm -> anode; cathode -> GND.
// Libraries: ArduinoMqttClient; WiFiS3 is supplied by the UNO R4 board package.
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <stdio.h>
#include <string.h>
#include "lab_config.h"

const char DEVICE_ID[] = "room1-actuator-01";
const char FIRMWARE_VERSION[] = "led-actuator-1.0.0";
const char COMMAND_TOPIC[] = "lab/room1/fan/set";
const char STATE_TOPIC[] = "lab/room1/fan/state";
const char STATUS_TOPIC[] = "lab/room1/actuator/status";
const int LED_PIN = LED_BUILTIN;
const unsigned long WIFI_RETRY_MS = 15000;
const unsigned long MQTT_RETRY_MS = 5000;

WiFiClient network;
MqttClient mqtt(network);
bool ledOn = false;
bool statePending = true;
bool wifiIdentityPrinted = false;
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;

bool sendText(const char* topic, const char* payload, bool retained) {
  if (!mqtt.beginMessage(topic, (unsigned long)strlen(payload), retained, 1)) return false;
  mqtt.print(payload);
  return mqtt.endMessage() == 1;
}

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  statePending = true;
}

void onCommand(int messageSize) {
  bool retained = mqtt.messageRetain();
  String topic = mqtt.messageTopic();
  char command[4] = {0};
  int index = 0;
  bool validSize = messageSize == 2 || messageSize == 3;
  while (mqtt.available()) {
    int value = mqtt.read();
    if (index < 3) command[index++] = (char)value;
  }
  if (topic != COMMAND_TOPIC || retained || !validSize) {
    Serial.println("IGNORED: unexpected topic, retained command, or invalid length.");
    return;
  }
  if (messageSize == 2 && strcmp(command, "ON") == 0) setLed(true);
  else if (messageSize == 3 && strcmp(command, "OFF") == 0) setLed(false);
  else { Serial.println("IGNORED: only exact ON/OFF accepted."); return; }
  Serial.print("RECEIVED "); Serial.print(COMMAND_TOPIC);
  Serial.print(" "); Serial.print(command);
  Serial.print(" -> LED "); Serial.println(ledOn ? "ON" : "OFF");
  // State publishing happens in loop(), outside this receive callback.
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
    if (ledOn) setLed(false);
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
  if (ledOn) setLed(false);  // OFF after a detected MQTT connection loss.
  if (now - lastMqttAttempt < MQTT_RETRY_MS) return false;
  lastMqttAttempt = now;
  if (!mqtt.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.print("MQTT connect error="); Serial.println(mqtt.connectError());
    return false;
  }
  if (!mqtt.subscribe(COMMAND_TOPIC, 1)) {
    Serial.println("SUBSCRIBE_FAILED");
    mqtt.stop();
    return false;
  }
  sendText(STATUS_TOPIC, "ONLINE", true);
  statePending = true;
  Serial.println("Ready: subscribed to lab/room1/fan/set; LED starts OFF.");
  return true;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);
  unsigned long started = millis();
  while (!Serial && millis() - started < 2000) delay(10);
  mqtt.setId(DEVICE_ID);
  mqtt.setCleanSession(true);
  mqtt.setKeepAliveInterval(60000);
  mqtt.setConnectionTimeout(3000);
  mqtt.onMessage(onCommand);
  if (MQTT_USERNAME[0] != '\0') mqtt.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);
  mqtt.beginWill(STATUS_TOPIC, (unsigned short)7, true, 1);
  mqtt.print("OFFLINE");
  mqtt.endWill();
  lastWifiAttempt = millis();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  if (!ensureConnected()) { delay(10); return; }
  mqtt.poll();
  if (statePending) {
    const char* state = ledOn ? "ON" : "OFF";
    if (sendText(STATE_TOPIC, state, true)) {
      statePending = false;
      Serial.print("REPORTED "); Serial.print(STATE_TOPIC); Serial.print(" "); Serial.println(state);
    }
  }
  delay(10);
}
