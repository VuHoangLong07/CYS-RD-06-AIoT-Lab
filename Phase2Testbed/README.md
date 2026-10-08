# Two UNO R4 WiFi boards + Modulino Distance MQTT testbed

Confirmed hardware: two Arduino UNO R4 **WiFi** boards and one Arduino Modulino
Distance **ABX00102**, connected with its bundled four-wire Qwiic cable.
Label the boards `room1-sensor-01` and `room1-actuator-01` before programming.

This revises the original ESP32/DHT22 plan: the measurement is now distance in
millimetres. ABX00102 does not measure temperature. Telemetry uses
`lab/room1/distance`; the command/state topics remain `lab/room1/fan/set` and
`lab/room1/fan/state`. The LED is a stand-in for a fan, not a powered fan.

## Parts and board roles

| Item | Role |
|---|---|
| UNO R4 WiFi #1 | Read the ABX00102 and publish distance |
| ABX00102 + bundled Qwiic cable | Plug into board #1's Qwiic socket |
| UNO R4 WiFi #2 | Subscribe to ON/OFF commands and drive its built-in L LED |
| Two USB-C data cables | Program/power the boards from your laptop |
| Laptop + reachable 2.4 GHz lab network | Run Mosquitto, Node-RED and Wireshark |
| Optional LED, 1 kOhm resistor, breadboard and jumpers | Larger external indicator on D7 |

The default actuator firmware uses `LED_BUILTIN`, so no external LED circuit
is needed for the first demonstration. This is the small **L** LED, distinct
from the power LED and the 12x8 matrix. See WIRING.md and wiring-diagram.png.

## 1. Connect and test the sensor locally

Unplug board #1 from USB. Connect the bundled Qwiic cable from its Qwiic socket
to either Qwiic socket on the Modulino Distance. Leave the cable intact.

| Signal carried inside the cable | UNO Qwiic endpoint | Module endpoint |
|---|---|---|
| Power | 3.3 V | 3V3 |
| Ground | GND | GND |
| Data | SDA on Wire1 | SDA |
| Clock | SCL on Wire1 | SCL |

These are electrical connections, not a connector pin-order instruction.
The keyed cable makes the mapping. Do not add wires to A4/A5 for this design:
the R4 WiFi's Qwiic socket uses **Wire1**, a separate bus. Do not connect the
module to 5 V. No extra DATA pull-up resistor is needed for this Qwiic setup.

In Arduino IDE:

1. Install **Arduino UNO R4 Boards** in Boards Manager.
2. Select **Arduino UNO R4 WiFi**, then the actual board's Tools > Port entry.
3. Install **Arduino_Modulino** in Library Manager and accept its dependencies.
   The API was checked against the current library source; record the version
   actually installed. Do not install a generic VL53L0X driver for this module.
4. Open `firmware/uno_r4_distance_test/uno_r4_distance_test.ino`, click Verify,
   then Upload. Open Serial Monitor at **115200 baud**.
5. Face the sensor toward a flat card/book about 100-500 mm away. Move it nearer
   and farther. The printed distance should change approximately as expected.

`Modulino.begin()` selects Wire1 automatically when the R4 WiFi board is selected.
This test checks sensor initialization before adding network software. If it
prints SENSOR_NOT_FOUND, unplug USB, reseat the cable, reconnect and reset.
If it prints NO_VALID_RANGE, check the target, optical opening, distance and
cable. Range depends on the target and environment; do not claim perfect ruler
accuracy. The specified nominal range is about 0-1200 mm. For a repeatable demo,
use an opaque flat target within a few hundred millimetres.

On Linux, discover ports with `ls /dev/ttyACM* /dev/ttyUSB*` and connect one board
at a time. If access is denied, check dialout membership; adding it with
`sudo usermod -aG dialout "$USER"` requires logout/login. Use a data-capable cable.

## 2. Test the actuator locally

Disconnect board #1 while identifying board #2's port. Select UNO R4 WiFi and
upload `firmware/uno_r4_led_test/uno_r4_led_test.ino`. The built-in L LED should
blink with one second ON and one second OFF. Reconnect both boards afterwards.

For an optional external LED, unplug USB and connect:

| From | To |
|---|---|
| UNO #2 D7 | One end of a 1 kOhm resistor |
| Other resistor end | LED anode, usually the long leg |
| LED cathode, usually short leg / flat side | UNO #2 GND |

Change `LED_PIN = LED_BUILTIN` to `LED_PIN = 7` in both actuator/LED-test sketches
and re-upload. The resistor is in series; never connect an LED directly to D7.
Keep board #1 and #2's power rails separate. No inter-board signal wire is needed.

## 3. Make Mosquitto reachable from both boards

Extract this folder to `~/iot-lab/mqtt-distance-testbed`. The Compose project
name remains `mqtt-software-lab` to reuse Phase 1's existing containers/volumes.
This is an update to that project, not a second broker. Preserve any local
customizations if your earlier deployment differs from the supplied files.

Phase 1 bound MQTT only to 127.0.0.1. Physical boards need the laptop's LAN IP:

```bash
ip -4 addr show scope global
ip route
cd ~/iot-lab/mqtt-distance-testbed
cp .env.example .env
nano .env
```

Replace the example with your laptop's actual reachable LAN address:

```dotenv
LAB_LAN_IP=192.168.1.10
MQTT_PORT=1883
NODE_RED_PORT=1880
```

Do not use 127.0.0.1, a Docker container IP, or either UNO's IP as the firmware's
broker endpoint. A DHCP reservation for the laptop helps keep the address stable.

```bash
sudo docker compose -f compose.yaml -f compose.physical.yaml config
sudo docker compose -f compose.yaml -f compose.physical.yaml up -d
sudo docker compose -f compose.yaml -f compose.physical.yaml ps
sudo docker compose -f compose.yaml -f compose.physical.yaml logs --tail=30 mosquitto
```

The extra Compose file adds the specified LAN address while keeping localhost
available to Python. Always use both -f files for subsequent Compose updates.
If 1883 is already occupied, change MQTT_PORT to 1884 and use 1884 in both
firmware headers and Python commands. Node-RED inside Docker still uses
`mosquitto:1883`; the broker's container-side port remains 1883.

Boards and laptop must be on a network with direct reachability. Use a 2.4 GHz
lab SSID without guest/client isolation or captive-portal login. This package
retains the anonymous Phase 1 broker for a controlled lab network. A shared
network calls for broker credentials/ACLs and matching client settings. Hardware
headers support authentication; these Python simulators do not yet implement it.
Docker published ports can bypass ordinary UFW input rules. Check routing,
binding and Docker firewall rules without disabling the entire firewall or
forwarding the broker from the Internet.

## 4. Import the distance dashboard and commission the software baseline

Open http://127.0.0.1:1880. In Manage palette, install
`@flowfuse/node-red-dashboard` if it is not installed. Import
`node-red-distance-flow.json` and Deploy. Disable earlier temperature/controller
flow tabs so only one controller publishes commands to fan/set.

Open http://127.0.0.1:1880/distance-lab/room1. It displays distance in mm,
reported LED state, control mode, manual ON/OFF, Automatic control and the
physical devices' MQTT status. The flow uses a separate dashboard path from
the earlier temperature dashboard. It starts in manual mode.

To run the distance simulator and Python actuator in two terminals:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python distance_publisher.py --interval 5
```

```bash
source .venv/bin/activate
python fan_subscriber.py
```

Use `--port 1884` on both if MQTT_PORT is 1884. Leave the physical boards
unpowered during this software-only baseline. The virtual actuator prints and
reports ON/OFF. Hardware-status widgets describe the hardware connections;
they do not monitor the Python clients.

The distance simulator emits 400, 350, 250, 180, 150, 220, 320, 400 mm every
five seconds. Automatic control uses hysteresis: **ON at <=200 mm**, **OFF at
>=300 mm**, hold the previous state between these thresholds. Manual control
is disabled while Automatic control is ON. A response to a physical distance
change may take up to roughly one publication interval plus network processing.

## 5. Upload the two MQTT programs

Install **ArduinoMqttClient** in Library Manager. WiFiS3 is supplied by the
UNO R4 board package. Do not upload a standalone ESP32 program to the UNO's
internal ESP32-S3 connectivity module.

For board #1 open `firmware/uno_r4_sensor/uno_r4_sensor.ino`; for board #2 open
`firmware/uno_r4_actuator/uno_r4_actuator.ino`. Each folder has a `lab_config.h`.
Edit both headers:

```cpp
const char WIFI_SSID[] = "YOUR_LAB_SSID";
const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
const char MQTT_HOST[] = "192.168.1.10";  // Your laptop's actual LAN IPv4.
const unsigned short MQTT_PORT = 1883;
```

Keep MQTT_USERNAME/PASSWORD empty for the unchanged anonymous lab broker.
Select the correct board and port for each upload. In Serial Monitor at 115200,
record DEVICE, FIRMWARE, BUILD, WIFI_FW, MAC and IP. They print after Wi-Fi
connects, even when the broker connection fails. Never display passwords in
the evidence video or submitted source headers.

Sensor client ID: `room1-sensor-01`, release `distance-sensor-1.0.0`.
Actuator client ID: `room1-actuator-01`, release `led-actuator-1.0.0`.
Keep the IDs distinct when cloning code onto further boards.

Example sensor schema only, not a measured result:

```json
{"device":"room1-sensor-01","firmware":"distance-sensor-1.0.0","seq":1,"uptime_ms":6000,"distance_mm":180,"unit":"mm","valid":true}
```

Both physical clients and the Python distance simulator use QoS 1. Telemetry
and commands are nonretained; output state and hardware ONLINE/OFFLINE status
are retained. The sensor skips invalid/unavailable readings; it does not invent
values. uptime_ms is a local elapsed-time value, not synchronized wall-clock time.

## 6. Replace one role at a time

| Stage | Sensor source | Actuator | Capture |
|---|---|---|---|
| S0 | Python distance publisher | Python fan subscriber | baseline-distance-simulated.pcapng |
| S1 | UNO #1 + ABX00102 | Python fan subscriber | baseline-distance-sensor.pcapng |
| S2 | UNO #1 + ABX00102 | UNO #2 built-in/external LED | baseline-distance-physical.pcapng |

For S1 press Ctrl+C in the distance publisher terminal and power board #1.
Leave board #2 unpowered. For S2 press Ctrl+C in the Python fan terminal and
power board #2. Never leave two publishers active for the same role. A baseline
from the old temperature exercise remains useful historical evidence, but
repeat S0 with distance data before directly comparing sensor traffic.

First keep Automatic control OFF, toggle ON then OFF, and check the L LED,
board #2 Serial log and reported state agree. Then enable Automatic control:
hold a flat target about 150 mm away for ON; move it to about 400 mm for OFF.
Hold each position at least 6-10 seconds. The gap between 200 and 300 mm prevents
rapid changes around a single threshold.

Actuator firmware starts OFF, accepts exact nonretained ON/OFF, ignores invalid
commands and retained command replay, and turns OFF after detected Wi-Fi/MQTT connection loss. Loss detection
is not instantaneous. Reported output state confirms software processing, not
an independent electrical measurement. A retained state/value is last known;
MQTT status tracks broker connectivity and does not certify sensor health.

## 7. Capture and collect evidence

Use the included Linux Docker capture helper from this project directory. It
captures real MQTT at the broker's eth0 using Wireshark's dumpcap engine:

```bash
bash capture_baseline.sh 300 baseline-distance-physical.pcapng
wireshark baseline-distance-physical.pcapng
```

Run matching filenames in S0 and S1 as well. See CAPTURE_BASELINE.md for package
installation and evidence-and-comparison.md for the repeated 300-second action
schedule and comparison table. Preserve the original baseline.pcapng. Use
`mqtt` as a Wireshark display filter; broker-side capture uses `tcp port 1883`.

Complete device-inventory.md with actual MAC/IP, module model, board/library
versions, firmware version/hash, date and wiring-photo ID. Submit the schematic
plus real wiring photos, redacted firmware, completed inventory, three captures,
run notes and a short physical demonstration. No supplied placeholder is evidence
of a measurement that has already happened.

## Relay: a later supervised stage

Use the LED for this build. A relay requires the exact module's supply/input
specifications, driver requirements, active level and startup behavior to be
checked by a supervisor. Do not drive a bare coil from an UNO GPIO. Restrict
that later exercise to supervised low-voltage DC; keep mains out of the testbed.

## Troubleshooting

| Symptom | Check |
|---|---|
| SENSOR_NOT_FOUND | Board #1 Qwiic socket, cable fully seated, Arduino_Modulino dependencies, UNO R4 WiFi selected; unplug/reconnect/reset |
| NO_VALID_RANGE / SENSOR_READ_FAILED | Opaque flat target, optical opening, range and Qwiic cable; module unplugging may require restart |
| Sensor not found with an I2C scanner | Scan Wire1, not A4/A5's Wire; default sensor address is 0x29 |
| Wi-Fi connects but MQTT fails | Laptop LAN IP/port, LAN Compose binding, AP isolation, broker logs and authentication |
| LED does not follow the switch | Automatic control OFF, correct board/firmware, one active actuator, watch small L LED rather than power LED |
| Distance display blank | Correct distance flow, numeric distance_mm + unit mm + valid true, correct telemetry topic |
| Values alternate between scripted and measured | Stop distance_publisher.py |
| Repeated MQTT disconnects | Unique client IDs, Wi-Fi reliability, power and broker logs |
| Display persists after device unplugged | Last known retained state; wait for status/connection detection and record it |

## Validation

See validation-report.md for desktop checks and their limits. The actual Arduino
board compile/upload, wiring and physical demonstration must be performed on
your laptop and boards. Real packet captures are created only by running the
capture procedure; this package does not contain fabricated captures.

## Primary references

- ABX00102 datasheet: https://docs.arduino.cc/resources/datasheets/ABX00102-datasheet.pdf
- Module: https://docs.arduino.cc/hardware/modulino-distance/
- UNO R4 WiFi: https://docs.arduino.cc/hardware/uno-r4-wifi/
- Modulino API and Wire1 selection: https://github.com/arduino-libraries/Arduino_Modulino/blob/main/src/Modulino.h
- MQTT API: https://github.com/arduino-libraries/ArduinoMqttClient/blob/master/src/MqttClient.h
- Dashboard: https://dashboard.flowfuse.com/getting-started.html
- Compose merge rules: https://docs.docker.com/reference/compose-file/merge/
- Capture: https://www.wireshark.org/docs/man-pages/dumpcap.html
- Docker firewall behavior: https://docs.docker.com/engine/install/ubuntu/

The phase1-temperature folder preserves the earlier simulator/dashboard source
for reference. Use the distance files at this folder's root for the revised lab.
