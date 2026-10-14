# Validation report

Checked for the revised user-confirmed design: two UNO R4 WiFi boards and one
Modulino Distance ABX00102. Date: 2026-10-07.

## Desktop checks completed

- All four .ino sketches passed g++ C++17 syntax checks with temporary mocked
  Arduino/WiFiS3/ArduinoMqttClient/Arduino_Modulino interfaces. API calls were
  cross-checked against official Arduino source. This is not a board build.
- The actual actuator sketch's command and connection handlers were exercised
  with mocked MQTT input/GPIO: exact ON/OFF, repeated ON, empty/long/lowercase/
  whitespace/embedded-NUL commands, wrong topic and retained replay; malformed
  inputs were ignored. GPIO became OFF after detected Wi-Fi or MQTT loss.
- The actual sensor sketch was exercised with mocked ranges/time/I2C: numeric
  distance_mm JSON, unit mm, valid true, QoS 1, nonretained telemetry and 5 s
  spacing. Invalid, outside-range or unavailable sensor results were omitted.
- Revised Python scripts passed syntax and offline Paho transport checks. The
  publisher produced the documented distance sequence/schema; the subscriber
  accepted exact ON/OFF and ignored invalid/retained-replay messages.
- The imported flow's JavaScript functions passed checks for 200/300 mm
  hysteresis, bad-data rejection, manual/automatic exclusion, mm formatting,
  state feedback and switch-enabling behavior. These checks ran the functions,
  not a live Node-RED browser session.
- Flow JSON IDs, wire targets, config references and dashboard path were checked.
  YAML files parsed successfully; their LAN/localhost port declarations were
  checked. The capture helper passed Bash syntax checking.
- Wiring-diagram.png was visually inspected; the mapping matches WIRING.md.

## Checks to complete on the real laptop and boards

Docker and Arduino CLI are not available in this execution environment, and no
physical UNO/sensor is connected here. No real board build, live broker session,
packet capture or physical demonstration is claimed for this revised kit.

1. Run `docker compose -f compose.yaml -f compose.physical.yaml config` and
   start the actual services on your laptop.
2. In Arduino IDE, select UNO R4 WiFi, record package/library versions and click
   Verify for each sketch. Upload the local tests, then the MQTT programs.
3. Confirm real changing sensor readings, manual ON/OFF and near/far automatic
   LED behavior. Record the actual MAC/IP, versions and result in the inventory.
4. Run the three real captures and fill the comparison table with measured data.

Shipped source hashes are in firmware/SHA256SUMS.txt. They cover unedited
sources and placeholder config headers. Recalculate after local changes and
redact credentials in source/screenshots submitted as evidence.
