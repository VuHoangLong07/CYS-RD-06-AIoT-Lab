# Device inventory: one ESP32 doing both MQTT roles

Replace TBD with actual observations. Example/preview values are not inventory data.

| Field | ESP32 PUB/SUB node |
|---|---|
| Asset label / client ID | esp32-room1-dual-01 |
| Board | Classic ESP32 development board; record exact module/board marking: TBD |
| Display | GMT130-V1.0, 240x240 ST7789; photograph labels |
| USB port at test time | TBD; earlier /dev/ttyUSB0 is not a permanent identifier |
| Firmware version | pubsub-tft-2.0.0 |
| Firmware source | firmware/esp32_pubsub_tft/esp32_pubsub_tft.ino |
| Config/source SHA-256 after local edits | TBD |
| Build date / time | TBD from Serial |
| ESP32 board package version | TBD |
| ESP-IDF SDK version | TBD from SDK Serial line |
| ArduinoMqttClient version | TBD |
| Adafruit GFX / ST7735-ST7789 / BusIO versions | TBD |
| TFT SPI mode / clock / rotation | Record tested mode: TBD; 10 MHz; rotation 0 |
| TFT wiring | GND-GND; VCC/BLK-3V3; SCK-18; SDA/MOSI-23; RES-22; DC-21; CS absent |
| Wi-Fi MAC | TBD from Serial/router |
| ESP32 IPv4 | TBD |
| Laptop broker IPv4 / host-side port | TBD; recheck rather than copying an old example |
| Lab SSID / network | TBD; password remains private |
| Role flags | ENABLE_PUBLISHER=TBD; ENABLE_SUBSCRIBER=TBD |
| Publish topics | lab/esp32/telemetry; lab/esp32/metrics; lab/esp32/events; fan/state when subscriber enabled; device/status |
| Subscribe topic | lab/room1/fan/set when enabled |
| Telemetry source | All environmental values simulated; uptime/RSSI/heap/runtime diagnostics from actual ESP32 |
| Output | TFT FAN indication; ACTUATOR_PIN=-1 by default, record optional LED if used |
| QoS / protocol | MQTT 3.1.1; publish QoS 1; record granted subscription QoS |
| Timing profile / seed | TBD; default variable-v2, seed 20261018; 5 s environment and 15 s metrics base intervals, bounded variation/events |
| Boot ID | TBD from Serial/JSON; separates counters across reboots |
| Power | USB -> ESP32; ESP32 3V3 -> TFT |
| Test date / operator / results | TBD |
| Live MQTT verification log / result | TBD; run verify_device.py, then independently observe the TFT |
| Wiring photo / demonstration video ID | TBD |

This replaces the two-UNO/distance-sensor variant with one classic ESP32 and
the earlier TFT. Both MQTT roles share one client ID and TCP connection. The
logical fan indication is not a physical fan measurement.

Record actual values and versions, then hash the tested source/configuration:

```bash
sha256sum firmware/esp32_pubsub_tft/esp32_pubsub_tft.ino
sha256sum firmware/esp32_pubsub_tft/display_config.h
```

The shipped firmware/SHA256SUMS.txt covers placeholder, unedited files. Keep a
private configured version and submit redacted lab_config.h without credentials.
Record that private vs redacted configuration hashes differ. Bump the firmware
version for behavior changes and keep the tested source alongside the capture.
