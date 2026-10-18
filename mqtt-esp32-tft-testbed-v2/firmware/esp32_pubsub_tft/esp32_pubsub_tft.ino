// Classic ESP32 + GMT130-V1.0 ST7789. One MQTT connection publishes and subscribes.
// All environmental fields are SIMULATED. Device diagnostics come from the ESP32.
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <esp_arduino_version.h>
#include <esp_random.h>
#include <stdio.h>
#include <string.h>
#include "lab_config.h"
#include "display_config.h"
#include "telemetry_model.h"

static_assert(PUBLISH_INTERVAL_MS >= 500 && PUBLISH_INTERVAL_MS <= 60000, "Use an environment base interval of 500..60000 ms");
static_assert(METRICS_INTERVAL_MS >= 1000 && METRICS_INTERVAL_MS <= 120000, "Use a metrics base interval of 1000..120000 ms");

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RESET);
WiFiClient network;
MqttClient mqtt(network);
RoomLab::Room room(MODEL_SEED);
RoomLab::Schedule traffic(MODEL_SEED, VARIABLE_TIMING, PUBLISH_INTERVAL_MS, METRICS_INTERVAL_MS);

const unsigned long WIFI_RETRY_MS = 15000;
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long STATE_RETRY_MS = 1000;
const unsigned long SCREEN_INTERVAL_MS = 250;
const uint16_t GREY = 0x9CF3;
const uint16_t ORANGE = 0xFD20;

unsigned long seq = 0, pubAckCount = 0, pubFailureCount = 0;
unsigned long rxCount = 0, ignoredCount = 0, stateAckCount = 0;
unsigned long metricsSeq = 0, eventSeq = 0, metricsAckCount = 0, eventAckCount = 0;
unsigned long metricsFailureCount = 0, eventFailureCount = 0, mqttSessions = 0;
unsigned long lastEnvAckMs = 0, maxEnvAckMs = 0, maxLoopGapMs = 0, previousLoop = 0;
bool occupancyEventPending = false;
uint8_t backgroundCursor = 0;
char bootId[9] = {0};
unsigned long lastPublishAttempt = 0, lastPublishAck = 0, lastReceive = 0;
unsigned long lastWifiAttempt = 0, lastMqttAttempt = 0, lastStateAttempt = 0;
unsigned long lastScreen = 0, stateRevision = 0;
float lastTemperature = 0;
bool hasPublished = false, hasReceived = false, hasSample = false;
bool subscribed = false, linkWasOnline = false, identityPrinted = false;
bool fanOn = false, statePending = true, receiveReadFailed = false;
int grantedQos = -1;
const char* pubStatus = "WAIT";
const char* stateStatus = "WAIT";
char lastCommand[4] = "--";

const int ROW_Y[] = {8, 33, 47, 65, 85, 99, 121, 143, 163, 177, 199, 211, 227};
const uint8_t ROW_SIZE[] = {2, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 1};
char rowCache[13][40] = {{0}};
uint16_t rowColour[13] = {0};

void drawRow(int index, const char* text, uint16_t colour, bool force) {
  char visible[38];
  snprintf(visible, sizeof(visible), "%.*s", (int)(224 / (6 * ROW_SIZE[index])), text);
  if (!force && strcmp(rowCache[index], visible) == 0 && rowColour[index] == colour) return;
  tft.fillRect(8, ROW_Y[index], 224, 8 * ROW_SIZE[index], ST77XX_BLACK);
  tft.setCursor(8, ROW_Y[index]);
  tft.setTextSize(ROW_SIZE[index]);
  tft.setTextColor(colour, ST77XX_BLACK);
  tft.print(visible);
  snprintf(rowCache[index], sizeof(rowCache[index]), "%s", visible);
  rowColour[index] = colour;
}

void drawScreen(bool refreshNow = false) {
  unsigned long now = millis();
  if (!refreshNow && now - lastScreen < SCREEN_INTERVAL_MS) return;
  lastScreen = now;
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  bool mqttUp = mqtt.connected();
  char text[40];
  drawRow(0, "ESP32 PUB + SUB", ST77XX_WHITE, false);
  snprintf(text, sizeof(text), "WiFi:%s MQTT:%s", wifiUp ? "UP" : "WAIT", mqttUp ? "ONLINE" : "OFFLINE");
  drawRow(1, text, mqttUp ? ST77XX_GREEN : ORANGE, false);
  String ip = wifiUp ? WiFi.localIP().toString() : String("--");
  snprintf(text, sizeof(text), "IP %s", ip.c_str());
  drawRow(2, text, GREY, false);
  drawRow(3, "PUB", ST77XX_CYAN, false);
  snprintf(text, sizeof(text), "%s OK:%lu ERR:%lu", ENABLE_PUBLISHER ? pubStatus : "DISABLED", pubAckCount, pubFailureCount);
  drawRow(4, text, !mqttUp ? ORANGE : (pubFailureCount ? ORANGE : ST77XX_GREEN), false);
  unsigned int sensorPage = (now / 4000) % 4;
  if (!hasSample) snprintf(text, sizeof(text), "SIM --.- C");
  else if (sensorPage == 0) snprintf(text, sizeof(text), "SIM %.1f C", lastTemperature);
  else if (sensorPage == 1) snprintf(text, sizeof(text), "SIM RH %.1f%%", room.humidity);
  else if (sensorPage == 2) snprintf(text, sizeof(text), "SIM %.0f lux", room.light);
  else snprintf(text, sizeof(text), "SIM CO2 %.0fppm", room.co2);
  drawRow(5, text, ST77XX_YELLOW, false);
  if ((now / 3000) % 2 == 1) snprintf(text, sizeof(text), "M:%lu E:%lu next:%.1fs", metricsAckCount, eventAckCount,
                                        traffic.environment.remaining((uint32_t)now) / 1000.0f);
  else if (hasPublished) snprintf(text, sizeof(text), "TX %lus  seq:%lu", (now - lastPublishAck) / 1000, seq);
  else snprintf(text, sizeof(text), "TX --  seq:%lu", seq);
  drawRow(6, text, GREY, false);
  drawRow(7, "SUB", ST77XX_CYAN, false);
  if (!ENABLE_SUBSCRIBER) snprintf(text, sizeof(text), "DISABLED (publisher-only stage)");
  else if (subscribed) snprintf(text, sizeof(text), "READY Q%d RX:%lu BAD:%lu", grantedQos, rxCount, ignoredCount);
  else snprintf(text, sizeof(text), "WAIT RX:%lu BAD:%lu", rxCount, ignoredCount);
  drawRow(8, text, subscribed ? ST77XX_GREEN : ORANGE, false);
  snprintf(text, sizeof(text), "FAN %s", fanOn ? "ON" : "OFF");
  drawRow(9, text, fanOn ? ST77XX_GREEN : GREY, false);
  if (hasReceived) snprintf(text, sizeof(text), "Last:%s %lus ago", lastCommand, (now - lastReceive) / 1000);
  else snprintf(text, sizeof(text), "Last:-- waiting for ON/OFF");
  drawRow(10, text, GREY, false);
  drawRow(11, COMMAND_TOPIC, GREY, false);
  snprintf(text, sizeof(text), "STATE:%s %s", stateStatus, FIRMWARE_VERSION);
  drawRow(12, text, stateStatus[0] == 'F' ? ST77XX_RED : GREY, false);
}

void initDisplay() {
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 240, TFT_SPI_MODE);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.drawFastHLine(8, 59, 224, GREY);
  tft.drawFastHLine(8, 137, 224, GREY);
  tft.drawFastHLine(8, 222, 224, GREY);
  drawScreen(true); // Display works even before Wi-Fi or MQTT connects.
}

void setFan(bool on) {
  fanOn = on;
  if (ACTUATOR_PIN >= 0) digitalWrite(ACTUATOR_PIN, on ? HIGH : LOW);
  ++stateRevision;
  statePending = true;
  lastStateAttempt = millis() - STATE_RETRY_MS; // New commands can report immediately.
  stateStatus = "WAIT";
}

void onCommand(int messageSize) {
  // Capture metadata BEFORE reading the last byte resets the library's RX state.
  String topic = mqtt.messageTopic();
  bool retainedReplay = mqtt.messageRetain() == 1;
  char command[4] = {0};
  unsigned long bytesRead = 0;
  unsigned long started = millis();
  bool readFailed = false;
  while (mqtt.available()) {
    if (millis() - started >= MQTT_TIMEOUT_MS) { readFailed = true; break; }
    int value = mqtt.read();
    if (value < 0) { readFailed = true; break; }
    if (bytesRead < 3) command[bytesRead] = (char)value;
    ++bytesRead;
  }
  bool validSize = messageSize == 2 || messageSize == 3;
  if (!ENABLE_SUBSCRIBER || topic != COMMAND_TOPIC || retainedReplay || !validSize ||
      readFailed || bytesRead != (unsigned long)messageSize) {
    ++ignoredCount;
    if (readFailed) receiveReadFailed = true; // Close outside this callback.
    Serial.println("RX_IGNORED: topic, retained replay, length or read failure.");
    return;
  }
  if (messageSize == 2 && strcmp(command, "ON") == 0) setFan(true);
  else if (messageSize == 3 && strcmp(command, "OFF") == 0) setFan(false);
  else { ++ignoredCount; Serial.println("RX_IGNORED: only exact ON/OFF accepted."); return; }
  ++rxCount;
  lastReceive = millis();
  hasReceived = true;
  snprintf(lastCommand, sizeof(lastCommand), "%s", command);
  Serial.print("RX "); Serial.print(COMMAND_TOPIC); Serial.print(" "); Serial.println(command);
  // Never publish here: endMessage() can poll and call us during a PUBACK wait.
}

bool publishText(const char* topic, const char* payload, bool retained) {
  size_t size = strlen(payload);
  if (!mqtt.beginMessage(topic, (unsigned long)size, retained, 1) ||
      mqtt.print(payload) != size || mqtt.endMessage() != 1) {
    mqtt.stop();
    return false;
  }
  return true; // QoS 1: broker PUBACK received, not proof of another client processing it.
}

void connectionLost() {
  if (linkWasOnline || subscribed || fanOn) {
    Serial.println("MQTT/WIFI lost; logical fan set OFF until a new command.");
    setFan(false);
  }
  linkWasOnline = false;
  subscribed = false;
  grantedQos = -1;
  pubStatus = "OFFLINE";
}

void printIdentity() {
  Serial.print("DEVICE="); Serial.println(DEVICE_ID);
  Serial.print("FIRMWARE="); Serial.println(FIRMWARE_VERSION);
  Serial.print("BUILD="); Serial.println(__DATE__ " " __TIME__);
  Serial.print("SDK="); Serial.println(ESP.getSdkVersion());
  Serial.print("MAC="); Serial.println(WiFi.macAddress());
  Serial.print("IP="); Serial.println(WiFi.localIP());
  Serial.print("TFT_MODE="); Serial.println(TFT_SPI_MODE);
  Serial.print("PUBLISHER="); Serial.println(ENABLE_PUBLISHER);
  Serial.print("SUBSCRIBER="); Serial.println(ENABLE_SUBSCRIBER);
  Serial.print("BOOT_ID="); Serial.println(bootId);
  Serial.print("MODEL_SEED="); Serial.println((unsigned long)MODEL_SEED);
  Serial.print("TIMING_PROFILE="); Serial.println(VARIABLE_TIMING ? "variable-v2" : "fixed-v2");
}

bool ensureConnected() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    identityPrinted = false;
    connectionLost();
    if (mqtt.connected()) mqtt.stop();
    if (now - lastWifiAttempt >= WIFI_RETRY_MS) {
      lastWifiAttempt = now;
      drawScreen(true);
      Serial.println("Connecting to lab Wi-Fi...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return false;
  }
  if (!identityPrinted) { printIdentity(); identityPrinted = true; }
  if (mqtt.connected()) return true;
  connectionLost();
  if (now - lastMqttAttempt < MQTT_RETRY_MS) return false;
  lastMqttAttempt = now;
  drawScreen(true);
  if (!mqtt.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.print("MQTT_CONNECT_FAILED error="); Serial.println(mqtt.connectError());
    return false;
  }
  if (ENABLE_SUBSCRIBER) {
    if (!mqtt.subscribe(COMMAND_TOPIC, 1)) {
      Serial.println("SUBSCRIBE_FAILED: check topic ACL and broker.");
      mqtt.stop();
      return false;
    }
    grantedQos = mqtt.subscribeQoS();
    subscribed = true;
    statePending = true;
    Serial.print("SUB_READY "); Serial.println(COMMAND_TOPIC);
  }
  if (!publishText(STATUS_TOPIC, "ONLINE", true)) return false;
  ++mqttSessions;
  traffic.connected((uint32_t)millis()); // Stagger streams; do not replay an outage backlog.
  occupancyEventPending = false;
  linkWasOnline = true;
  pubStatus = "WAIT";
  Serial.println("MQTT_READY: one connection, independent publisher and subscriber roles.");
  return true;
}

void publishPendingState() {
  unsigned long now = millis();
  if (!ENABLE_SUBSCRIBER || !statePending || now - lastStateAttempt < STATE_RETRY_MS) return;
  lastStateAttempt = now;
  unsigned long snapshotRevision = stateRevision;
  const char* state = fanOn ? "ON" : "OFF";
  stateStatus = "SEND";
  drawScreen(true);
  if (publishText(STATE_TOPIC, state, true)) {
    ++stateAckCount;
    // A newer command may arrive DURING the QoS 1 acknowledgment wait.
    if (stateRevision == snapshotRevision) { statePending = false; stateStatus = "ACK"; }
    else stateStatus = "WAIT";
    Serial.print("STATE_ACK "); Serial.println(state);
  } else stateStatus = "FAIL";
}

void publishTelemetry() {
  unsigned long now = millis();
  if (!ENABLE_PUBLISHER || !traffic.environment.due((uint32_t)now)) return;
  unsigned long interval = seq ? now - lastPublishAttempt : 0;
  lastPublishAttempt = now;
  lastTemperature = room.temperature;
  ++seq;
  hasSample = true;
  char payload[896];
  int length = snprintf(payload, sizeof(payload),
    "{\"schema\":2,\"device\":\"%s\",\"firmware\":\"%s\",\"boot_id\":\"%s\","
    "\"seq\":%lu,\"uptime_ms\":%lu,\"temperature\":%.2f,\"unit\":\"C\",\"sensor\":\"simulated\","
    "\"humidity_pct\":%.1f,\"light_lux\":%.1f,\"pressure_hpa\":%.2f,\"co2_ppm\":%.1f,\"occupied\":%s,"
    "\"simulated_fields\":[\"temperature\",\"humidity_pct\",\"light_lux\",\"pressure_hpa\",\"co2_ppm\",\"occupied\"],"
    "\"fan_state\":\"%s\",\"rssi_dbm\":%ld,\"free_heap_bytes\":%lu,"
    "\"publish_gap_ms\":%lu,\"timing_profile\":\"%s\",\"model_seed\":%lu,\"reason\":\"%s\"}",
    DEVICE_ID, FIRMWARE_VERSION, bootId, seq, now, lastTemperature,
    room.humidity, room.light, room.pressure, room.co2, room.occupied ? "true" : "false",
    fanOn ? "ON" : "OFF", (long)WiFi.RSSI(), (unsigned long)ESP.getFreeHeap(), interval,
    VARIABLE_TIMING ? "variable-v2" : "fixed-v2", (unsigned long)MODEL_SEED, traffic.reason);
  pubStatus = "SEND";
  drawScreen(true);
  unsigned long ackStarted = millis();
  if (length > 0 && (size_t)length < sizeof(payload) && publishText(TELEMETRY_TOPIC, payload, false)) {
    lastEnvAckMs = millis() - ackStarted;
    if (lastEnvAckMs > maxEnvAckMs) maxEnvAckMs = lastEnvAckMs;
    ++pubAckCount;
    lastPublishAck = millis();
    hasPublished = true;
    pubStatus = "ACK";
    Serial.print("PUB_ACK "); Serial.print(TELEMETRY_TOPIC); Serial.print(" "); Serial.println(payload);
  } else {
    ++pubFailureCount;
    pubStatus = "FAIL";
    Serial.println("PUB_FAILED: no broker acknowledgment; check connection.");
  }
  traffic.environmentAttempted((uint32_t)millis());
}

void publishMetrics() {
  unsigned long now = millis();
  ++metricsSeq;
  char payload[896];
  int length = snprintf(payload, sizeof(payload),
    "{\"schema\":2,\"device\":\"%s\",\"firmware\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu,"
    "\"uptime_ms\":%lu,\"source\":\"esp32\",\"free_heap_bytes\":%lu,\"min_free_heap_bytes\":%lu,"
    "\"rssi_dbm\":%ld,\"wifi_channel\":%ld,\"mqtt_sessions\":%lu,"
    "\"env_pub_ack_count\":%lu,\"env_pub_fail_count\":%lu,\"metrics_pub_ack_count\":%lu,"
    "\"metrics_pub_fail_count\":%lu,\"event_pub_ack_count\":%lu,\"event_pub_fail_count\":%lu,"
    "\"rx_count\":%lu,\"rejected_count\":%lu,\"state_ack_count\":%lu,"
    "\"last_env_ack_ms\":%lu,\"max_env_ack_ms\":%lu,\"max_loop_gap_ms\":%lu,"
    "\"fan_state\":\"%s\",\"timing_profile\":\"%s\",\"model_seed\":%lu}",
    DEVICE_ID, FIRMWARE_VERSION, bootId, metricsSeq, now,
    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (long)WiFi.RSSI(),
    (long)WiFi.channel(), mqttSessions, pubAckCount, pubFailureCount, metricsAckCount,
    metricsFailureCount, eventAckCount, eventFailureCount, rxCount, ignoredCount, stateAckCount,
    lastEnvAckMs, maxEnvAckMs, maxLoopGapMs, fanOn ? "ON" : "OFF",
    VARIABLE_TIMING ? "variable-v2" : "fixed-v2", (unsigned long)MODEL_SEED);
  if (length > 0 && (size_t)length < sizeof(payload) && publishText(METRICS_TOPIC, payload, false)) {
    ++metricsAckCount;
    Serial.print("METRICS_ACK "); Serial.println(payload);
  } else { ++metricsFailureCount; Serial.println("METRICS_FAILED"); }
  traffic.metricsAttempted((uint32_t)millis());
}

void publishOccupancyEvent() {
  ++eventSeq;
  char payload[384];
  int length = snprintf(payload, sizeof(payload),
    "{\"schema\":2,\"device\":\"%s\",\"firmware\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu,"
    "\"uptime_ms\":%lu,\"event\":\"occupancy_changed\",\"source\":\"simulated\","
    "\"occupied\":%s,\"revision\":%lu}", DEVICE_ID, FIRMWARE_VERSION, bootId, eventSeq,
    (unsigned long)millis(), room.occupied ? "true" : "false", (unsigned long)room.revision);
  if (length > 0 && (size_t)length < sizeof(payload) && publishText(EVENTS_TOPIC, payload, false)) {
    ++eventAckCount;
    occupancyEventPending = false;
    Serial.print("EVENT_ACK "); Serial.println(payload);
  } else { ++eventFailureCount; Serial.println("EVENT_FAILED"); }
}

void publishBackground() {
  if (!ENABLE_PUBLISHER) return;
  // Fair selection of ONE background publication per loop, not an unbounded burst.
  for (uint8_t offset = 0; offset < 3; ++offset) {
    uint8_t stream = (backgroundCursor + offset) % 3;
    uint32_t now = (uint32_t)millis();
    if ((stream == 0 && traffic.environment.due(now)) ||
        (stream == 1 && traffic.metrics.due(now)) || (stream == 2 && occupancyEventPending)) {
      backgroundCursor = (stream + 1) % 3;
      if (stream == 0) publishTelemetry();
      else if (stream == 1) publishMetrics();
      else publishOccupancyEvent();
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  snprintf(bootId, sizeof(bootId), "%08lx", (unsigned long)esp_random());
  room.begin((uint32_t)millis());
  if (ACTUATOR_PIN >= 0) { pinMode(ACTUATOR_PIN, OUTPUT); digitalWrite(ACTUATOR_PIN, LOW); }
  initDisplay(); // Keep TFT initialization before all network connection attempts.
  mqtt.setId(DEVICE_ID);
  mqtt.setCleanSession(true);
  mqtt.setKeepAliveInterval(60000);
  mqtt.setConnectionTimeout(MQTT_TIMEOUT_MS);
  mqtt.onMessage(onCommand);
  if (MQTT_USERNAME[0] != '\0') mqtt.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);
  mqtt.beginWill(STATUS_TOPIC, (unsigned short)7, true, 1);
  mqtt.print("OFFLINE");
  mqtt.endWill();
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  network.setConnectionTimeout(MQTT_TIMEOUT_MS);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  lastWifiAttempt = millis();
  lastPublishAttempt = millis();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  unsigned long now = millis();
  unsigned long loopGap = now - previousLoop;
  previousLoop = now;
  if (loopGap > maxLoopGapMs) maxLoopGapMs = loopGap;
  if (room.update((uint32_t)now, fanOn)) {
    occupancyEventPending = true; // Latest occupancy only; never queue an offline backlog.
    traffic.occupancyChanged((uint32_t)now);
  }
  if (!ensureConnected()) { drawScreen(); delay(2); return; }
  mqtt.poll();
  if (receiveReadFailed) { receiveReadFailed = false; mqtt.stop(); }
  if (!mqtt.connected()) { connectionLost(); drawScreen(); delay(2); return; }
  publishPendingState();
  if (mqtt.connected()) {
    mqtt.poll();
    if (receiveReadFailed) { receiveReadFailed = false; mqtt.stop(); }
    if (mqtt.connected()) publishBackground();
  }
  if (!mqtt.connected()) connectionLost();
  drawScreen();
  delay(2); // No five-second delay: poll MQTT and refresh status between publications.
}
