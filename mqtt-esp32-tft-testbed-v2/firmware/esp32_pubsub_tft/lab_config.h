#pragma once
#include <stdint.h>
// Configure your local copy. Keep real credentials out of shared evidence.
const char WIFI_SSID[] = "YOUR_2_4_GHZ_SSID";
const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
const char MQTT_HOST[] = "YOUR_LAPTOP_LAN_IP"; // Recheck the laptop IP; not the ESP32 IP.
const unsigned short MQTT_PORT = 1883;
const char MQTT_USERNAME[] = ""; // Fill both fields if your broker requires auth.
const char MQTT_PASSWORD[] = "";

const char DEVICE_ID[] = "esp32-room1-dual-01";
const char FIRMWARE_VERSION[] = "pubsub-tft-2.0.0";
const char TELEMETRY_TOPIC[] = "lab/esp32/telemetry"; // Preserve your earlier ESP32 topic.
const char METRICS_TOPIC[] = "lab/esp32/metrics";
const char EVENTS_TOPIC[] = "lab/esp32/events";
const char COMMAND_TOPIC[] = "lab/room1/fan/set";
const char STATE_TOPIC[] = "lab/room1/fan/state";
const char STATUS_TOPIC[] = "lab/room1/device/status";

const bool ENABLE_PUBLISHER = true;
const bool ENABLE_SUBSCRIBER = true; // Set false for the optional publisher-only S1 stage.
const unsigned long PUBLISH_INTERVAL_MS = 5000;
const unsigned long METRICS_INTERVAL_MS = 15000;
const bool VARIABLE_TIMING = true; // false = fixed 5 s environment / 15 s metrics; no event bursts.
const uint32_t MODEL_SEED = 20261018; // Record/change this for repeatable model runs.
const unsigned long MQTT_TIMEOUT_MS = 3000;
// -1 means the logical fan is displayed on the TFT; no GPIO load is connected.
// For a verified external LED circuit, set 4: GPIO4 -> 1 kOhm -> anode; cathode -> GND.
const int ACTUATOR_PIN = -1;
