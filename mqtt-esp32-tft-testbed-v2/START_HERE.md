# Start the ESP32 + TFT v2 testbed

Already have a working setup? Follow UPGRADE_V2.md to preserve your local network
credentials and working display configuration while upgrading.

Use one classic ESP32 and your GMT130-V1.0 240 x 240 ST7789 screen. The ESP32
publishes `lab/esp32/telemetry`, subscribes to `lab/room1/fan/set`, and shows both
roles on the TFT. Temperature, humidity, light, pressure, CO2 and occupancy are labelled simulations.
Uptime, RSSI, heap and runtime diagnostics come from the physical ESP32. The default actuator is the TFT's FAN ON/OFF status.

## 1. Wire and check the display

Unplug USB while wiring. Connect these **printed pin labels**:

| TFT | ESP32 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCK | GPIO18 |
| SDA / MOSI | GPIO23 |
| RES | GPIO22 |
| DC | GPIO21 |
| BLK | 3V3 |
| CS | No wire; this module has no CS |

Use wiring-diagram.png as your reference. SDA here means SPI data. The screen
does not use I2C. The optional LED circuit is in WIRING.md; no relay is needed.

Install **esp32 by Espressif Systems** in Arduino IDE Boards Manager. Select
**ESP32 Dev Module**, your actual USB port, and Serial Monitor **115200 baud**.
Install **Adafruit GFX Library**, **Adafruit ST7735 and ST7789 Library**,
**Adafruit BusIO**, and **ArduinoMqttClient** in Library Manager.

Upload `firmware/esp32_tft_test/esp32_tft_test.ino`. Expect TFT TEST OK, colour
bars and a changing counter. Both sketch folders use your earlier SPI_MODE3,
10 MHz, rotation 0 settings. If the proven working mode on your actual screen
is MODE0, set it in both display_config.h files. Fix the standalone display
before continuing.

## 2. Configure and start the laptop services

On your Linux laptop, open a terminal **inside the extracted project folder**:

```bash
python3 configure_lab.py
bash labctl.sh up
```

Enter the laptop's current Wi-Fi IPv4 address, the 2.4 GHz SSID and the Wi-Fi
password. The helper suggests the current wlo1 address when available; verify
it with `ip -4 addr show dev wlo1`. It saves the same broker IP/host port in
.env and firmware lab_config.h and hides password entry. It preserves the
existing role flags, MQTT credentials and other firmware settings. Custom
MQTT/NODE_RED port values already in .env are preserved.

Docker Engine/Desktop and the Compose plugin must already be installed. The
helper uses your existing Docker access or sudo and starts the Phase 1 project
with both Compose files. It preserves its named volumes. This supplied broker
uses anonymous MQTT on the controlled lab network. For a previously configured
authenticated broker, preserve that configuration and set matching credentials
in firmware, Node-RED and your test clients; labctl's MQTT shortcuts assume the
supplied anonymous configuration.

The ESP32 and laptop must be able to reach each other over the Wi-Fi network.
MQTT host values depend on where the client runs:

| Client | MQTT host | Port |
|---|---|---|
| ESP32 firmware | Laptop's current LAN IPv4 | MQTT_PORT from .env, usually 1883 |
| Node-RED in Docker | mosquitto | 1883 |
| Python on the laptop | 127.0.0.1 | MQTT_PORT from .env |
| labctl MQTT shortcuts | 127.0.0.1 inside Mosquitto | 1883 |

## 3. Upload the combined firmware and import the dashboard

Open `firmware/esp32_pubsub_tft/esp32_pubsub_tft.ino` with its three .h files
still alongside it. Verify and upload. Both role flags default to true; if you
previously ran S1, restore ENABLE_SUBSCRIBER=true. The display should show
WiFi UP, MQTT ONLINE, increasing environmental PUB ACK/OK count and SUB READY Q1.
Its SIM row rotates through temperature, humidity, light and CO2; M/E counters
track diagnostics/events, and next shows the next environmental deadline.

Open http://127.0.0.1:1880. In Node-RED Manage palette, install
`@flowfuse/node-red-dashboard` if missing. Import `node-red-esp32-flow.json`,
disable old controller tabs that publish fan/set, and Deploy. Open
http://127.0.0.1:1880/esp32-lab/room1. If NODE_RED_PORT differs from 1880,
replace 1880 in both URLs. Keep Automatic control OFF for the following check.
Stop the old Python publisher/actuator and disconnect old boards using these
topics, so the feedback comes from this ESP32.

## 4. Check both roles

In one terminal:

```bash
bash labctl.sh watch
```

In another terminal in the project folder, send ON and observe the TFT, then OFF:

```bash
bash labctl.sh on
```

```bash
bash labctl.sh off
```

PUB OK should keep increasing. Each accepted command raises RX, changes FAN,
updates Last ON/OFF and produces a STATE ACK. The watcher should show matching
fan/state feedback. Commands are exact uppercase ON/OFF and nonretained.

For a repeatable live check and a log of actual observations:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python verify_device.py --output device-check.json
```

Use `--port 1884` if your host-side MQTT port is 1884. The check waits for live
telemetry from esp32-room1-dual-01, sends ON and OFF, requires new nonretained
state feedback, and requires increasing telemetry sequence numbers after each
command. It ignores retained replay, finishes OFF, and refuses to overwrite an
existing log. Use `--device` if you changed DEVICE_ID. For broker authentication,
set the MQTT_PASSWORD environment variable privately and add `--username`.

A PASS confirms those MQTT observations. Independently watch/record the actual
TFT: the script cannot see your screen or prove an external fan is moving.

Now enable Automatic control on the dashboard: ON at >=28 C, OFF at <=26 C,
holding state between them. The new synthetic room changes slowly, so allow
several minutes for crossings. Environmental reports usually arrive every
4-6 seconds, metrics every 12-18 seconds, with occupancy events, short follow-up
bursts and occasional longer environmental gaps. UPGRADE_V2.md describes this
configurable application schedule.

## 5. Capture the normal session and record evidence

Install Wireshark/dumpcap on the laptop as described in CAPTURE_BASELINE.md.
Use a separate terminal for the timed capture and follow its normal-session
schedule while it runs:

```bash
bash capture_baseline.sh 300 baseline-esp32-pubsub.pcapng
wireshark baseline-esp32-pubsub.pcapng
```

Use display filter `mqtt`; if using a different host-side port, the broker-side
capture still uses 1883. Start capture before reconnecting/powering the ESP32 to
include CONNECT, SUBSCRIBE/SUBACK and early publishes. Keep disconnect/error
tests out of the normal capture. The helper refuses to overwrite existing files.

Use README.md section 7 for simulated -> publisher-only -> combined-role captures
and evidence-and-comparison.md for the comparison. Preserve any existing Phase 1
baseline.pcapng. Fill device-inventory.md with the actual MAC/IP, board marking,
firmware/core/library versions and working display mode. Record source hashes
after local configuration; redact credentials in shared firmware evidence.

Your final evidence set is the wiring diagram and real wiring photo, redacted
firmware source, completed device inventory, real packet captures, device-check
log and a video of the TFT showing increasing PUB/RX counts and ON/OFF while
publishing continues. The included preview is a legacy v1 layout illustration, not a hardware photo.

The deliverable is complete. Arduino target compilation/upload, physical
observations and actual captures are the steps you carry out on your laptop.
