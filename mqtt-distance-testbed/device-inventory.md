# Device inventory

The board/module models are user-confirmed. Replace TBD with observations;
no MAC, IP, test result or source hash below is invented.

| Field | Sensor node | Actuator node |
|---|---|---|
| Asset label / MQTT client ID | room1-sensor-01 | room1-actuator-01 |
| Board | Arduino UNO R4 WiFi | Arduino UNO R4 WiFi |
| Board marking / serial / wiring photo ID | TBD | TBD |
| Application MCU / radio | RA4M1 / ESP32-S3 connectivity module | RA4M1 / ESP32-S3 connectivity module |
| Function | Read distance and publish JSON | Subscribe to ON/OFF and drive LED |
| Attached component | Arduino Modulino Distance ABX00102 | Built-in L LED, or record external LED |
| Sensor chip / nominal range | VL53L4CDV0DH/1 / about 0-1200 mm | Not applicable |
| Wiring / interface | Bundled Qwiic cable, 3.3 V, Wire1, I2C 0x29 | LED_BUILTIN; optional D7 + 1 kOhm + LED to GND |
| Firmware release | distance-sensor-1.0.0 | led-actuator-1.0.0 |
| Firmware source | firmware/uno_r4_sensor/uno_r4_sensor.ino | firmware/uno_r4_actuator/uno_r4_actuator.ino |
| Source SHA-256 after final local edits | TBD | TBD |
| Build date/time printed in Serial | TBD | TBD |
| Arduino UNO R4 Boards package version | TBD | TBD |
| ArduinoMqttClient version | TBD | TBD |
| Arduino_Modulino + driver dependency versions | TBD | Not used |
| Connectivity firmware version (WIFI_FW) | TBD | TBD |
| Wi-Fi MAC from Serial/router | TBD | TBD |
| IPv4 / DHCP reservation | TBD | TBD |
| Broker laptop IPv4 / laptop-side port | TBD | TBD |
| SSID / lab network (password private) | TBD | Same network |
| Power | USB-C | USB-C |
| Publish topics | lab/room1/distance; lab/room1/sensor/status | lab/room1/fan/state; lab/room1/actuator/status |
| Subscribe topic | None | lab/room1/fan/set |
| Protocol / QoS | MQTT 3.1.1 over TCP / QoS 1 | MQTT 3.1.1 over TCP / QoS 1 |
| Sampling / state behavior | Target 5 s, invalid readings omitted | Boot OFF; exact nonretained ON/OFF; OFF after detected connection loss |
| Test date / operator / results | TBD | TBD |

Label both boards physically and take a photo linking each label to its board.
Record versions from Boards Manager/Library Manager and firmware Serial output.
USB port names can change; they are not permanent network identities.

Hash the final .ino files after configuration/code edits:

```bash
sha256sum firmware/uno_r4_sensor/uno_r4_sensor.ino
sha256sum firmware/uno_r4_actuator/uno_r4_actuator.ino
```

The shipped firmware/SHA256SUMS.txt records the unedited sources and template
headers. Archive a private configured copy; submit redacted headers without
Wi-Fi/MQTT passwords. Identify the redaction if comparing hashes of shared and
private configuration files. Increment firmware versions when behavior changes.

Design substitution to record: two UNO R4 WiFi boards + distance sensor replace
the original two-ESP32/DHT22 temperature plan. This demonstrates the same MQTT
workflow with distance measurements, not DHT22/temperature evidence.
