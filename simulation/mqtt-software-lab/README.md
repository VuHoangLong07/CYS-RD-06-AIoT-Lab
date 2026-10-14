# MQTT room-control lab: no hardware required

This project runs a complete MQTT application on one laptop. Docker Compose
runs Mosquitto and Node-RED. Two Python programs run on the laptop: one replaces
a temperature sensor, and one replaces a fan. Node-RED sends fan commands in
response to temperature messages.

This is a starting point for an IoT testbed. It does not include an IDS or machine
learning. The generated temperatures follow a scripted sequence; the virtual
fan prints its state rather than changing the temperature sequence.

## What each part does

| Part | Purpose |
|---|---|
| Docker Compose | Starts and connects the two container services |
| Mosquitto | MQTT broker: routes messages by topic |
| Temperature publisher | Sends JSON readings every 5 seconds |
| Node-RED | Reads temperatures, makes decisions, sends commands |
| Fan subscriber | Receives ON/OFF and reports the virtual fan state |

| Topic | Payload | Publisher | Subscriber |
|---|---|---|---|
| `lab/room1/temperature` | JSON with numeric `temperature` | Python sensor | Node-RED |
| `lab/room1/fan/set` | Plain text `ON` or `OFF` | Node-RED or manual test | Python fan and Node-RED debug |
| `lab/room1/fan/state` | Plain text `ON` or `OFF` | Python fan | Node-RED debug |

A topic is a routing name, not a file or URL. A publisher sends a message;
a subscriber asks the broker to forward messages with matching topic names.

## 1. Check prerequisites

On Linux Mint, open Terminal:

```bash
docker --version
docker compose version
sudo docker info
python3 --version
```

If Docker and Compose are already installed, keep your existing installation.
The commands below use `sudo docker` so changing Docker group membership is
unnecessary. If `docker` already works without sudo, you can omit sudo throughout.

If Docker or Compose is missing, use the Docker installation instructions linked
at the end of this guide. For Mint, use its Ubuntu base codename, not the Mint
codename, when configuring an Ubuntu repository. Mint 22.x is based on Ubuntu
Noble (24.04). Check your actual base with:

```bash
cat /etc/os-release
```

For the Python environment and ZIP extraction, if needed:

```bash
sudo apt update
sudo apt install python3-venv python3-pip unzip
```

## 2. Extract and enter the project

Download `mqtt-software-lab.zip`, place it in Downloads, then:

```bash
mkdir -p ~/iot-lab
unzip ~/Downloads/mqtt-software-lab.zip -d ~/iot-lab
cd ~/iot-lab/mqtt-software-lab
```

The folder contains `compose.yaml`, `mosquitto/mosquitto.conf`,
`temperature_publisher.py`, `fan_subscriber.py`, `requirements.txt`,
`node-red-flow.json`, and this guide.

## 3. Start Mosquitto and Node-RED

```bash
sudo docker compose up -d
sudo docker compose ps
sudo docker compose logs --tail=30
```

The first launch downloads the images and needs Internet access. Both services
should show as running. If Node-RED is still starting, check its logs until it
reports that the server is running.

- `-d` runs the containers in the background.
- Mosquitto is available to laptop programs at `127.0.0.1:1883`.
- The Node-RED editor is available at http://127.0.0.1:1880.
- Inside Docker, Node-RED reaches Mosquitto using `mosquitto:1883`.
- Named volumes keep broker data and Node-RED flows when containers are stopped
  or recreated.

The broker allows anonymous MQTT connections for this local learning setup.
Both published ports bind to `127.0.0.1`. If you later make it accessible to
other devices, add authentication before changing those bindings.

## 4. Prepare Python once

In the project directory:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

The virtual environment keeps this project's Python packages separate from
Linux Mint's system Python. `paho-mqtt` is the MQTT client library.

## 5. Import the ready-made Node-RED flow

1. Open http://127.0.0.1:1880 in your browser.
2. Open the top-right menu, then choose **Import** (or press Ctrl+I).
3. Choose **select a file to import**, then select `node-red-flow.json`.
4. Import the flow and click **Deploy**.
5. Open the **Debug** sidebar on the right (the bug icon).
6. The MQTT nodes should display **connected** below them.

The flow uses only built-in nodes; no dashboard or extra palette package is
required. Import it once. Docker's Node-RED volume saves it after deployment.

The automatic path is:

`MQTT in (temperature) -> JSON -> Fan controller -> MQTT out (fan/set)`

The JSON node turns the incoming JSON string into an object. The controller
reads `msg.payload.temperature` and applies this rule:

| Temperature | Fan command |
|---|---|
| At least 28 C | ON |
| At most 26 C | OFF |
| Between 26 and 28 C | Keep the previous requested state; initially OFF |

The gap between the thresholds is hysteresis: it prevents repeated switching
near a single threshold. A command is sent for every valid reading so a fan
subscriber that starts later receives a current command on the next reading.
Commands set a state rather than toggle it, making duplicates harmless.

The lower branches contain manual ON/OFF Inject buttons and debug subscribers
for commands and fan state.

**Important address distinction:** `127.0.0.1` inside the Node-RED container
means Node-RED's own container. Its MQTT broker must be **mosquitto**, port
**1883**. The imported flow already uses this setting.

## 6. Start the virtual fan in Terminal A

```bash
cd ~/iot-lab/mqtt-software-lab
source .venv/bin/activate
python fan_subscriber.py
```

Wait for:

```text
Connected to 127.0.0.1:1883; virtual fan is OFF.
Ready: subscribed to lab/room1/fan/set; waiting for ON/OFF.
```

Keep this terminal open. It is the software replacement for a device listening
for commands. It publishes its initial state as OFF.

## 7. Test a manual command before starting the sensor

In Node-RED, click the small square beside **Manual fan ON**. Terminal A should
show a line ending with:

```text
RECEIVED lab/room1/fan/set ON -> FAN ON (changed)
```

The Node-RED Debug sidebar should also display a fan-state message `ON`.
Click **Manual fan OFF** and check that the virtual fan reports OFF.

You can also send commands from another terminal in the project folder:

```bash
sudo docker compose exec mosquitto mosquitto_pub -h 127.0.0.1 -t lab/room1/fan/set -m ON -q 1
sudo docker compose exec mosquitto mosquitto_pub -h 127.0.0.1 -t lab/room1/fan/set -m OFF -q 1
```

These commands use the MQTT command-line client already inside the Mosquitto
container; there is no need to install a separate host MQTT client.

## 8. Start the temperature publisher in Terminal B

```bash
cd ~/iot-lab/mqtt-software-lab
source .venv/bin/activate
python temperature_publisher.py
```

It sends this repeatable sequence every 5 seconds:

```text
24, 25, 27, 29, 30, 28, 26, 25, then repeat
```

Each reading is a JSON message, for example:

```json
{"device":"sim-room1","seq":4,"timestamp":"2026-09-30T04:00:00+00:00","temperature":29.0,"unit":"C"}
```

`seq` counts readings within this publisher run. The timestamp is UTC. When
29 C arrives (about 15 seconds after the first reading), Node-RED sends ON.
At 26 C (about 30 seconds after the first reading), it sends OFF.

Look at all three places:

- Terminal B: `SENT lab/room1/temperature ...`
- Node-RED Debug: parsed readings, fan commands, and fan state.
- Terminal A: `FAN ON` and `FAN OFF`.

The manual buttons remain available, but the automatic controller sends another
command at the next temperature reading. Stop the publisher with Ctrl+C before
doing isolated manual tests.

## 9. Force a specific temperature

Stop the running publisher first with Ctrl+C. In Terminal B:

```bash
python temperature_publisher.py --value 30 --count 3
python temperature_publisher.py --value 24 --count 3
```

The first command sends three high readings and the fan should become ON.
The second sends three low readings and it should become OFF. These commands
stop automatically; without `--count`, the program keeps running.

To monitor every lab message in another terminal:

```bash
cd ~/iot-lab/mqtt-software-lab
sudo docker compose exec mosquitto mosquitto_sub -h 127.0.0.1 -t 'lab/#' -v
```

`#` matches every topic below `lab/`, and `-v` prints both topic and payload.
Use Ctrl+C to stop monitoring.

## 10. Shut down and resume

Stop both Python programs with Ctrl+C, then:

```bash
sudo docker compose down
```

Named volumes are kept by this command. To resume, run `sudo docker compose
up -d`, open Node-RED, and restart the two Python programs. Do not add `-v`
to the down command unless you intentionally want to delete saved volumes and
flows.

## MQTT details you are testing

- Telemetry and commands use QoS 1: the broker acknowledges them, but duplicates
  are possible. A broker acknowledgment does not by itself prove the fan acted.
- Fan state is sent after the subscriber processes a command, giving separate
  application-level confirmation.
- Commands and temperature readings are **not retained**. A new subscriber
  waits for a fresh message.
- Fan state **is retained**. A new state subscriber immediately sees the last
  reported state. This is the last known state, not an indication that the fan
  program is currently connected.
- Temperature generation is repeatable, which makes demonstrations easier to
  compare. It is not a physical model of a heated/cooled room.

## Troubleshooting

### Port 1883 is already in use

You may already have Mosquitto running directly on Linux Mint. Check:

```bash
sudo ss -ltnp | rg ':1883|:1880'
```

If `rg` is unavailable, use `grep -E ':1883|:1880'` instead. Keep your existing
broker and use another laptop port for this lab:

Create a file named `.env` in this project folder containing:

```dotenv
MQTT_PORT=1884
```

Then use the usual `sudo docker compose up -d`. Start both Python programs with
`--port 1884`. Node-RED still uses `mosquitto:1883`, and commands executed
inside the Mosquitto container still use 1883.

If 1880 is occupied, add `NODE_RED_PORT=1881` to `.env` and open
http://127.0.0.1:1881 instead.

### Connection refused

Check `sudo docker compose ps` and `sudo docker compose logs --tail=50
mosquitto`. Check the Python `--port` matches the laptop-side MQTT port.

### Node-RED MQTT node says disconnected

Double-click the MQTT node, edit its broker configuration, and check server
`mosquitto`, port `1883`, no TLS, and no username/password. Then Deploy.

### Node-RED sees commands but Terminal A does not

Start `fan_subscriber.py` and wait for its Ready message. Verify that the topic
is exactly `lab/room1/fan/set`. MQTT topics are case-sensitive. The command
payload must be plain text ON or OFF, not a JSON object.

### Temperature JSON appears but the fan never changes

Check that the Fan controller is wired to the MQTT output and Deploy has been
clicked. The JSON needs a numeric `temperature`, for example `29`, not the
string `"29"`. Check the Debug sidebar for errors.

## Official documentation

- Mosquitto Docker configuration: https://github.com/eclipse-mosquitto/mosquitto/blob/master/docker/generic/README.md
- Node-RED in Docker: https://nodered.org/docs/getting-started/docker
- Importing Node-RED flows: https://nodered.org/docs/user-guide/editor/workspace/import-export
- Paho Python MQTT client: https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html
- Docker Engine installation: https://docs.docker.com/engine/install/ubuntu/
- Docker Compose networking: https://docs.docker.com/compose/how-tos/networking/

Python dependency: `paho-mqtt==2.1.0`. Node-RED image: `4.1.15`.
Mosquitto uses the moving `2` major-version tag. For strict experiment
reproducibility, record the image IDs/digests actually pulled along with your
run date; this ZIP alone does not lock the Mosquitto image digest.

## Validation of this starter

The Python sources, Compose YAML, Node-RED import JSON, and flow wire references
passed static checks. The two Python scripts and the supplied flow were also
run together using Node-RED 4.1.15 and a temporary local aMQTT broker. All eight
simulated readings produced the expected commands and fan-state reports.
Threshold boundaries, hysteresis, both manual Inject buttons, and invalid
command handling passed.

Docker was unavailable in the authoring environment, so the actual Docker
Compose startup and Mosquitto container were not executed there. The temporary
validation broker is not a dependency of this project. Use the launch and
verification steps above on your laptop.
