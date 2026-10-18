# Phase 2 v2: rich ESP32 telemetry, variable timing and TFT status

For an existing build, start with **UPGRADE_V2.md**. For a new build, use **START_HERE.md**. It includes a
configuration helper, Docker/MQTT shortcuts and a repeatable live device check.

Hardware: your classic ESP32 Dev Module and GMT130-V1.0, 240x240 ST7789 TFT.
One ESP32 uses one MQTT connection to publish telemetry, subscribe to ON/OFF
commands and report the resulting logical fan state. The TFT displays both roles.

This build preserves the earlier display wiring and the old telemetry topic
`lab/esp32/telemetry`. The current command/state topics remain
`lab/room1/fan/set` and `lab/room1/fan/state`. Import the new flow: the earlier
distance flow listens to a different topic and cannot control this build.

Only the ESP32 and screen are required. Environmental values are **simulated**: temperature, humidity, light, pressure,
CO2 and occupancy. They are labelled SIM on the TFT/dashboard and declared in
`simulated_fields` in JSON. Uptime, Wi-Fi RSSI, heap and runtime diagnostics
come from the physical ESP32. The default fan state is an ON/OFF indication on the
screen; no physical fan or relay is connected. This is physical MQTT endpoint
and display evidence, not DHT22 or ambient-temperature measurement evidence.

## 1. Wire with USB unplugged

| TFT label | ESP32 endpoint | Function |
|---|---|---|
| GND | GND | Ground |
| VCC | 3V3 | Module power |
| SCK | GPIO18 | SPI clock |
| SDA | GPIO23 | SPI MOSI data, not I2C SDA |
| RES | GPIO22 | Display reset |
| DC | GPIO21 | Data/command select |
| BLK | 3V3 | Backlight power |
| CS | No wire; module has no CS pin | Firmware uses CS = -1 |

Power the ESP32 by USB. Use the board's printed GPIO labels; exact header
positions depend on its manufacturer. Do not connect the TFT logic to 5 V.
Your earlier blank-screen problem involved jumper/breadboard contacts, so use
the known-good wires/breadboard. A lit backlight alone does not prove SPI works.
See WIRING.md and wiring-diagram.png.

The optional external indicator uses GPIO4 -> 1 kOhm resistor -> LED anode;
LED cathode -> ESP32 GND. Set ACTUATOR_PIN=4 only after wiring that circuit.
The default ACTUATOR_PIN=-1 needs no additional parts and uses the TFT indication.

## 2. Install Arduino support and test the screen first

In Arduino IDE, install **esp32 by Espressif Systems** in Boards Manager and
select **ESP32 Dev Module**. Use the actual Tools > Port entry; the earlier
/dev/ttyUSB0 may change after reconnection. Serial Monitor is **115200 baud**.

Install in Library Manager:

- Adafruit GFX Library.
- Adafruit ST7735 and ST7789 Library.
- Adafruit BusIO, including dependencies when prompted.
- ArduinoMqttClient.

WiFi and SPI are supplied by the ESP32 board package. This sketch uses
ArduinoMqttClient so publisher success can mean a QoS 1 broker PUBACK; the older
PubSubClient sketch's QoS 0 TX OK meant local publish-call success.

Open `firmware/esp32_tft_test/esp32_tft_test.ino`, Verify and Upload. You should
see TFT TEST OK, red/green/blue bars and a counter changing once per second.
Resolve this local test before adding Wi-Fi/MQTT.

The saved standalone test used SPI_MODE3, 10 MHz and rotation 0. Both included
display_config.h files start with those settings. The later combined sketch
had SPI_MODE0 as its configurable default. If your actual screen works with
MODE0, change TFT_SPI_MODE in **both** sketch folders to SPI_MODE0. Keep the
wiring and all other settings fixed while testing that single change. The
included sketches initialize the TFT before attempting network connections.

## 3. Recheck the broker's laptop LAN address

Your last reported laptop Wi-Fi address was 10.1.0.82, but DHCP/network changes
can change it. Recheck on the Linux Mint laptop:

```bash
ip -4 addr show dev wlo1
```

Use the current `inet` IPv4 address without the /prefix. MQTT_HOST is the
laptop's address, not the ESP32's address or a Docker bridge address.

Extract this folder to `~/Documents/Swin/IotProject/mqtt-esp32-tft-testbed`:

```bash
cd ~/Documents/Swin/IotProject/mqtt-esp32-tft-testbed
python3 configure_lab.py
bash labctl.sh up
```

The helper asks for the current laptop LAN address and Wi-Fi credentials. It
sets LAB_LAN_IP in .env and MQTT_HOST in lab_config.h consistently. Password
entry is hidden. Existing port values, broker credentials and firmware role
flags are preserved. Keep MQTT_PORT=1883 and NODE_RED_PORT=1880 unless your
existing project uses another published port. For manual configuration, copy
.env.example to .env, edit it, and set the matching firmware host/port yourself.

The equivalent Docker commands are:

```bash
sudo docker compose -f compose.yaml -f compose.physical.yaml config
sudo docker compose -f compose.yaml -f compose.physical.yaml up -d
sudo docker compose -f compose.yaml -f compose.physical.yaml ps
sudo docker compose -f compose.yaml -f compose.physical.yaml logs --tail=30 mosquitto
```

This reuses the Phase 1 project name mqtt-software-lab and its named volumes.
It updates that project rather than starting a second broker. The physical
override adds one LAN binding and preserves localhost for Python. Keep using
both -f files for later updates. If your existing deployment has custom broker
authentication/configuration, preserve it rather than replacing it with this
starter's anonymous configuration.

The ESP32 needs a reachable 2.4 GHz lab Wi-Fi network, without captive-portal
login or client isolation between it and the laptop. The starter anonymous
broker is for a controlled lab network. Docker published ports can bypass
ordinary UFW input rules; check Docker/routing/firewall behavior if needed.
Do not disable the entire firewall as a connectivity fix.

## 4. Configure and upload the combined sketch

Open `firmware/esp32_pubsub_tft/esp32_pubsub_tft.ino`. If you used
configure_lab.py, its Wi-Fi and broker fields are already set. Otherwise edit
these fields in lab_config.h manually:

```cpp
const char WIFI_SSID[] = "YOUR_2_4_GHZ_SSID";
const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
const char MQTT_HOST[] = "10.1.0.82"; // Use the address you just rechecked.
const unsigned short MQTT_PORT = 1883;
```

Leave MQTT_USERNAME/PASSWORD empty for the supplied anonymous broker. If your
existing broker requires authentication, fill both, and configure the same
account in Node-RED. Do not reuse an old IP or password from an earlier example.

Default role flags are both true:

```cpp
const bool ENABLE_PUBLISHER = true;
const bool ENABLE_SUBSCRIBER = true;
```

Verify and Upload with ESP32 Dev Module selected. Serial Monitor prints DEVICE,
FIRMWARE, BUILD, SDK, MAC, IP, TFT_MODE and role flags after Wi-Fi connects.
Keep the client ID esp32-room1-dual-01 unique among active MQTT clients.

The display should show WiFi UP, MQTT ONLINE, PUB ACK with increasing OK count,
and SUB READY Q1. SUB READY confirms subscription acknowledgment; RX remains
zero until an accepted ON/OFF command arrives. Both roles use one connection;
you do not need a second ESP32 or a second client ID for the subscriber.

## What the TFT shows

| Display field | Meaning |
|---|---|
| WiFi / MQTT | Current detected connection state |
| IP | ESP32's assigned IP, distinct from the laptop broker IP |
| PUB WAIT / SEND / ACK / OFFLINE | Telemetry publication state |
| PUB OK / ERR | Telemetry broker-acknowledged count / failed attempts since boot |
| SIM sensor row | Rotates temperature, humidity, light and CO2; all environmental values are simulated |
| TX age / seq / M / E / next | Environmental acknowledgment age/sequence; alternates with metrics/event acknowledgment counts and next environmental deadline |
| SUB READY Q1 | Broker accepted the subscription at QoS 1 |
| RX / BAD | Accepted command count / rejected or unreadable messages |
| FAN ON/OFF | Current logical state applied to the TFT and optional LED GPIO |
| Last ON/OFF and age | Most recent accepted command and time since receipt |
| STATE WAIT / SEND / ACK / FAIL | Publication of the state-feedback message |

Counters restart at boot. MQTT loss turns the logical state OFF after detection
and resubscribes on reconnection. The screen can still show the last received
command ON while the current FAN state is OFF after connection loss; those are
different facts. Loss detection is not instantaneous. Broker PUBACK does not
prove Node-RED or another subscriber processed the message. State ACK confirms
broker receipt of the device's separate application feedback.

The screen-preview image is the legacy v1 layout illustration from desktop test
fixtures. Version 2 keeps the same layout and adds rotating sensor fields and
M/E/next counters. It is not a photograph of your ESP32.

## 5. Import the rebuilt Node-RED dashboard

Open http://127.0.0.1:1880. Install @flowfuse/node-red-dashboard in Manage palette
if missing, import node-red-esp32-flow.json and Deploy. Disable older temperature
and distance controller flow tabs using fan/set so only one controller acts.

Open http://127.0.0.1:1880/esp32-lab/room1. The flow has a manual switch,
Automatic control, SIM temperature, logical fan state and ESP32 broker status.
Its Docker broker setting is mosquitto:1883, even if the host publishes 1884.
Hardware uses the laptop LAN IP and host-side port. These are different contexts.

The flow starts in MANUAL. For manual tests keep Automatic control OFF. Enable
it explicitly for automatic control: ON at temperature >=28 C; OFF at <=26 C;
hold the previous state in between. Version 2 uses a slowly changing synthetic room model rather than an eight-value
sequence. Its temperature follows a roughly four-minute cycle, with modest
occupancy and logical-fan effects. Allow several minutes to see automatic
transitions. See UPGRADE_V2.md for the model, sources and bounded timing profile.

| Topic | Producer | Consumer / payload |
|---|---|---|
| lab/esp32/telemetry | ESP32 | Node-RED; rich environmental JSON, usually every 4-6 s |
| lab/esp32/metrics | ESP32 | Node-RED; real device diagnostics, usually every 12-18 s |
| lab/esp32/events | ESP32 | Node-RED; simulated occupancy-change events, roughly every 30-90 s |
| lab/room1/fan/set | Node-RED or test CLI | ESP32; exact ON or OFF, nonretained |
| lab/room1/fan/state | ESP32 | Node-RED; plain ON/OFF, retained |
| lab/room1/device/status | ESP32 / broker Last Will | Node-RED; retained ONLINE/OFFLINE |

The complete schema and example messages are in UPGRADE_V2.md. The existing
flat numeric temperature and unit=C remain compatible with the fan controller.

uptime_ms is elapsed time, not synchronized wall-clock time. Device status
tracks broker connectivity; retained state is last known, not proof of current
connectivity or an independent measurement of a physical fan.

## 6. Prove PUB and SUB separately, then together

Install mosquitto-clients if the commands are missing. Use your current broker
IP in place of the example. First stop Python publishers/actuators and disconnect
the previous UNO boards. Keep only this ESP32 active on the roles.

Terminal A:

```bash
mosquitto_sub -h 10.1.0.82 -p 1883 -t 'lab/#' -q 1 -v
```

You should see variable-rate environmental telemetry, slower metrics and
occupancy events while the TFT PUB OK count rises. A brief event burst and an
occasional longer gap are intentional in the variable profile.
With dashboard AUTO OFF, use Terminal B to send ON, observe, then OFF:

```bash
mosquitto_pub -h 10.1.0.82 -p 1883 -t 'lab/room1/fan/set' -q 1 -m 'ON'
```

```bash
mosquitto_pub -h 10.1.0.82 -p 1883 -t 'lab/room1/fan/set' -q 1 -m 'OFF'
```

Observe RX increasing, Last ON/OFF, FAN state on the TFT, STATE ACK and matching
fan/state in Terminal A. Do not add -r to actuator commands. The firmware ignores
retained command replay delivered on subscription and only accepts exact
uppercase payloads. Live MQTT 3.1.1 deliveries do not necessarily preserve the
publisher's RETAIN flag; publish commands nonretained at the source.

The same commands are available without host mosquitto-clients as
`bash labctl.sh watch`, `bash labctl.sh on` and `bash labctl.sh off`. They run
inside the supplied anonymous Mosquitto container and always use port 1883
there. Each helper uses both Compose files.

After installing requirements.txt in the virtual environment below, run
`python verify_device.py --output device-check.json` with AUTO off. It requires
fresh telemetry, ON/OFF feedback and continuing telemetry after each command,
then saves actual observations. Add --port if your host publishes another
port. This check does not inspect the TFT; observe it yourself during the run.

Enable Automatic control on the dashboard and watch the SIM temperature drive
commands while publication continues. This is the combined-role demonstration.

If authentication is enabled, the firmware/Node-RED must use it too. The supplied
Python scripts accept --username and --password-env MQTT_PASSWORD. Keep passwords
in a local environment/configuration, not shared screenshots or command history.

## 7. Optional one-role-at-a-time comparison

| Stage | Publisher | Subscriber/actuator | Capture |
|---|---|---|---|
| S0 | Python temperature_publisher.py | Python fan_subscriber.py | baseline-esp32-simulated.pcapng |
| S1 | ESP32 with ENABLE_SUBSCRIBER=false | Python fan_subscriber.py | baseline-esp32-publisher.pcapng |
| S2 | ESP32 with both flags true | Same ESP32, TFT indication | baseline-esp32-pubsub.pcapng |

The default sketch is already S2. For S0, unpower it and run the Python scripts
in separate terminals. For S1, stop the Python publisher and upload the combined
sketch with ENABLE_SUBSCRIBER=false. For S2, stop the Python actuator, set the
subscriber flag back to true and re-upload. Do not leave multiple state publishers
or multiple telemetry sources active on these topics.

Python setup:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python temperature_publisher.py --interval 5
```

In another terminal, activate the environment and run python fan_subscriber.py.
Add --port 1884 to both if that is the published host port. These revised
simulators use the same three telemetry topics, model seed and scheduling rules
as the firmware. The Python node reports null for ESP32-only measurements and
does not feed the virtual actuator state into its room model. See the comparison
notes for differences; matching settings do not imply byte-identical traffic.

## 8. Capture and evidence

Run the capture helper from this project directory on the rootful Linux Docker
host. It captures at the broker's eth0 with Wireshark's dumpcap engine:

```bash
bash capture_baseline.sh 300 baseline-esp32-pubsub.pcapng
wireshark baseline-esp32-pubsub.pcapng
```

Use matching stage filenames for S0/S1. Preserve earlier baseline.pcapng and
distance captures. See CAPTURE_BASELINE.md and evidence-and-comparison.md for
installation, the timed normal-session schedule and comparison notes. Do not
mix deliberate disconnect/invalid-command tests into the normal baseline.

Complete device-inventory.md with real MAC/IP, board marking, firmware/core/
library versions, SPI mode, source hashes and test results. Collect the wiring
diagram and real photos, configured firmware with redacted credentials, captures,
notes and a video showing rising PUB/RX counters and ON/OFF while PUB continues.
No included example IP, reading or preview is a measured hardware result.

## Troubleshooting

| Symptom | Check |
|---|---|
| TFT blank even with test sketch | Actual pin labels, 3V3 and BLK, known-good jumpers/breadboard, no CS, matching MODE3/MODE0 |
| Test works, combined screen blank | Same display_config.h in both folders, correct sketch/port, reset/power, Serial boot messages |
| Wi-Fi never UP | 2.4 GHz SSID/password, signal, captive portal; the initialized TFT should still show WAIT |
| Wi-Fi UP, MQTT OFFLINE | Current laptop IP, host port/LAN mapping, broker logs/authentication, AP client isolation |
| PUB ACK but SUB RX zero | READY subscription, correct fan/set topic, send exact nonretained ON/OFF, disable AUTO for manual tests |
| RX rises but TFT switches back | AUTO still active, another controller, MQTT loss setting state OFF |
| Node-RED does not act automatically | New /esp32-lab dashboard, AUTO enabled, numeric temperature JSON, telemetry topic and MQTT nodes connected |
| Commands/state appear but TFT does not change | Stop virtual fan/old boards; check ESP32 Serial RX and the expected combined firmware |
| Duplicate MQTT disconnects | Unique client ID; do not clone it onto another live board |

## Implementation and validation

The loop polls MQTT continuously between scheduled publications. It uses
unsigned elapsed-time timers, bounded startup offsets, jitter, event follow-ups
and occasional scheduled gaps. No reporting interval is implemented as a long
delay(). Background streams are selected fairly, one per loop. Incoming commands only update local state and mark a
feedback message pending. Publishing happens outside the callback. A revision
counter preserves a newer command arriving while an older state awaits PUBACK.
Network connect/acknowledgment calls can briefly pause the loop; this is a lab
prototype, not a hard real-time actuator controller.

See validation-report.md for desktop checks. Arduino target compilation/upload,
actual screen behavior, live Node-RED and real captures must run on your laptop.
The package contains no fabricated packet captures.

## Primary references

- ST7789 constructor/init APIs: https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7789.h
- TFT SPI clock API: https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_SPITFT.h
- MQTT acknowledgment and subscription behavior: https://github.com/arduino-libraries/ArduinoMqttClient/blob/master/src/MqttClient.cpp
- ESP32 device metrics: https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/Esp.h
- FlowFuse dashboard: https://dashboard.flowfuse.com/getting-started.html
- Capture: https://www.wireshark.org/docs/man-pages/dumpcap.html
- Python MQTT callbacks/acknowledgments: https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html

## Version 2 model tests

Run `python3 -m unittest discover -s tests -v`. With g++ available, compile
`g++ -std=c++17 tests/test_firmware_model.cpp -o /tmp/room-model-check` and run
`/tmp/room-model-check`. These test model behaviour on the desktop, not ESP32
target compilation or physical networking.
