# Upgrade the existing ESP32 + TFT testbed to version 2

The same ESP32 now publishes six environmental fields, device diagnostics and
occupancy-change events. It remains the ON/OFF subscriber and keeps the same
TFT wiring. This upgrade needs no additional hardware.

## What changed

| Stream | Topic | Data | Default application schedule |
|---|---|---|---|
| Room environment | lab/esp32/telemetry | Temperature, humidity, light, pressure, CO2, occupancy, logical fan state; RSSI/heap/uptime | Usually 4-6 s between scheduled attempts |
| Device diagnostics | lab/esp32/metrics | Heap/minimum heap, RSSI/channel, MQTT sessions, acknowledgment/failure/RX counters, environment PUBACK duration, largest loop gap | Usually 12-18 s |
| Occupancy events | lab/esp32/events | Occupancy changed, boolean value, revision, event sequence | On simulated occupancy transitions, approximately 30-90 s apart |
| Commands | lab/room1/fan/set | Exact ON or OFF | Received whenever the controller sends them |
| State feedback | lab/room1/fan/state | Plain ON/OFF | After commands and reconnection; retained |
| Connectivity | lab/room1/device/status | ONLINE/OFFLINE | Connection status / Last Will; retained |

**Temperature, humidity, light, pressure, CO2 and occupancy are all simulated.**
JSON declares them in `simulated_fields`; sensor values on the TFT/dashboard
are labelled SIM. They are not DHT22, CO2-sensor or physical occupancy readings.
RSSI, channel, heap, uptime and runtime counters/timings come from the real ESP32.
The logical fan state describes the TFT indication and optional configured LED,
not an independently measured physical fan.

The old numeric `temperature`, `unit: C`, `sensor: simulated`, device identity
and telemetry topic remain available, so the existing 28/26 C fan controller
continues to work. Version 2 adds `schema: 2`, a boot ID, new fields and topics.

## Timing behaviour

The default is a bounded synthetic application profile, inspired by ordinary
periodic and event-driven reporting. It has not been fitted to measured IoT
deployment traffic. Periodic reporting is also common in real devices; timing
randomness alone is not proof of realism.

- Environmental base interval: 5000 ms, with uniform jitter from 4000 to 6000 ms.
- After about 4% of ordinary environment attempts, the next scheduled gap is
  7500-10000 ms. This is application scheduling, not injected packet loss.
- An occupancy transition schedules an environment reading after 400-900 ms
  and two follow-ups 800-1400 ms apart. It also produces one occupancy event.
- Device diagnostics use a 15000 ms base with 12000-18000 ms variation.
- First environment and diagnostics attempts are staggered after MQTT connection:
  800-1800 ms and 2000-5000 ms respectively.
- After reconnection, the schedule restarts without replaying missed reports or
  stale occupancy events. Pending occupancy changes are coalesced to the latest
  value while offline; there is no unbounded event queue.

These are **application scheduling targets**. Actual capture intervals also
include loop execution, TFT work, QoS acknowledgments, Wi-Fi and TCP behaviour.
One MQTT message is not necessarily one TCP packet or one Wi-Fi frame.

Each stream has its own sequence counter. An environmental `publish_gap_ms`
records elapsed time between environment attempts, with 0 for the first one.
Failed attempts can leave sequence gaps. QoS 1 can produce duplicates. Counters
restart at reboot; use boot_id and device to separate runs.

The room model changes gradually: temperature has a roughly four-minute cycle;
humidity varies with temperature; light and CO2 respond gradually to simulated
occupancy. The local logical fan modestly changes the synthetic temperature and
CO2 targets. A room model is not a calibrated physical model.

## Upgrade your working hardware build

1. Back up your working firmware folder, .env and exported Node-RED flow.
   Keep the existing v1 baseline captures.
2. Extract the new package into a **separate folder**. The new combined sketch
   folder contains four files: esp32_pubsub_tft.ino, telemetry_model.h,
   lab_config.h and display_config.h. Keep all four together.
3. Copy your actual Wi-Fi/broker settings into the **new** lab_config.h, or run
   `python3 configure_lab.py` there. Also preserve any custom MQTT username,
   password, port, device ID and role settings. Do not copy the old lab_config.h
   over the new file: it lacks the new topics and schedule settings.
4. Preserve your **working display_config.h** from the old combined sketch.
   The known wire map is unchanged. Keep your proven SPI mode, clock and rotation;
   do not change a working screen while testing this publisher upgrade.
5. In Arduino IDE select ESP32 Dev Module and your actual USB port. Open the new
   esp32_pubsub_tft.ino, Verify, and Upload. The libraries are unchanged:
   ArduinoMqttClient, Adafruit GFX, ST7735/ST7789 and BusIO, plus the ESP32 core.
6. Serial Monitor at 115200 should print `FIRMWARE=pubsub-tft-2.0.0`, a boot ID,
   timing profile and model seed. Verify WiFi UP, MQTT ONLINE and SUB READY Q1.
7. Replace the old ESP32 Node-RED flow with node-red-esp32-flow.json and Deploy.
   Preserve your custom broker authentication. Keep only one active fan
   controller and one dashboard configuration for this route. The page remains
   **http://127.0.0.1:1880/esp32-lab/room1**, using your configured editor port.
8. Check manual ON/OFF with AUTO off, then enable AUTO. The new dashboard shows
   the environmental fields, reporting gap, diagnostics and latest occupancy event.
   The model changes slowly; allow several minutes for threshold crossings.

There is no need to replace your working Compose deployment or custom capture/
attack scripts for this firmware upgrade. The package's capture helper includes
the stdout-redirection fix for your earlier output-file permission error.

## TFT behaviour

PUB OK/ERR still counts **environmental** attempts only. The SIM row rotates
temperature, relative humidity, light and CO2 every four seconds. The small TX
row alternates acknowledgment age/sequence with M/E acknowledgment counts and
the next environment deadline. SUB RX, last command and FAN ON/OFF remain visible.

M means diagnostics publications; E means occupancy-event publications. The old
screen-preview picture is a v1 layout illustration, not a hardware observation.

## Configure repeatable comparisons

The new lab_config.h includes:

```cpp
const unsigned long PUBLISH_INTERVAL_MS = 5000;
const unsigned long METRICS_INTERVAL_MS = 15000;
const bool VARIABLE_TIMING = true;
const uint32_t MODEL_SEED = 20261018;
```

Set VARIABLE_TIMING=false for fixed 5 s environment and 15 s diagnostics
intervals without short environment follow-ups or scheduled gaps. Occupancy
events still occur when the simulated occupancy changes. Set true for the
variable profile above. Change the seed deliberately and record it in run notes.

The seed controls synthetic room values and scheduling randomness. Network,
startup and loop differences still affect real packet times; a seed does not
make separate captures identical. The Python model and firmware scheduling rules
match in controlled desktop tests, with ordinary floating-point tolerances.

The software publisher is upgraded as well:

```bash
source .venv/bin/activate
python temperature_publisher.py --profile variable --seed 20261018
```

For a fixed-rate comparison use `--profile fixed`. `--interval 5` selects the
environment base; it does not disable jitter in variable mode. `--count N`
counts environmental messages, while metrics/events may also be published.
`--value 29` remains available for a controlled temperature override.

Python publishes null for ESP32-only measurements instead of inventing RSSI or
microcontroller heap values. It does not observe the virtual actuator state;
its room model uses logical fan=false. Record that difference when comparing
software, publisher-only and combined hardware stages.

## Check and capture

Keep old publishers/actuators stopped while the ESP32 owns these roles. From
the laptop you can watch its streams using the current broker IP:

```bash
mosquitto_sub -h YOUR_LAPTOP_LAN_IP -p 1883 -q 1 -t 'lab/esp32/#' -v
```

Keep AUTO off for `python verify_device.py --output device-check-v2.json`.
The helper accepts the extended schema, checks core PUB/SUB behaviour and allows
30 seconds per step. The default seed/profile ordinarily produces many
environment gaps, diagnostics messages and several occupancy events in a
five- to ten-minute observation.

Capture a fresh normal v2 session with no workload generator running:

```bash
capture_file="baseline-esp32-v2-variable-$(date -u +%Y%m%dT%H%M%SZ).pcapng"
bash capture_baseline.sh 600 "$capture_file"
wireshark "$capture_file"
```

Use separate display filters for `mqtt.topic == "lab/esp32/telemetry"`,
`mqtt.topic == "lab/esp32/metrics"` and `mqtt.topic == "lab/esp32/events"`.
Count device-to-broker publications rather than adding forwarded subscriber
copies. Use capture timestamps for network intervals, not unsynchronized uptime.

Record profile, seed, schema/firmware, boot ID, versions, normal interval
distribution, event/burst counts and payload sizes. Preserve v1 captures as a
different baseline version. Hardware diagnostics and application behaviour are
measurements; timing profile names, identifiers and run labels should not be
treated as evidence that an IDS has learned an attack pattern.

## Verification scope

The package includes dependency-free Python model tests and a native C++ model
test. Desktop checks also exercised the actual sketch against temporary API
mocks for commands during PUBACK waits, failed acknowledgments, reconnection,
stream fairness, screen bounds and timer rollover. They do not replace an ESP32
target build, upload, live dashboard check or a real capture on your laptop.

API references: [ArduinoMqttClient](https://github.com/arduino-libraries/ArduinoMqttClient/blob/master/src/MqttClient.h),
[ESP32 device metrics](https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/Esp.h),
[ESP32 random API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/random.html).
