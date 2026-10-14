# MQTT software lab with a live dashboard

Run a complete room-control application on one Linux Mint laptop. Docker
Compose runs Mosquitto and Node-RED. Python replaces the temperature sensor
and fan. The dashboard displays the sensor value and provides an ON/OFF
control. Wireshark records the normal MQTT session.

No physical sensor or fan is needed. The temperature sequence is scripted;
the fan changes its reported state without changing the temperature sequence.
This project does not yet include an IDS or machine learning.

## Messages and clients

| Topic | Payload | Publisher | Subscriber |
|---|---|---|---|
| `lab/room1/temperature` | JSON with numeric `temperature` | Python sensor | Node-RED |
| `lab/room1/fan/set` | Plain text ON or OFF | Node-RED | Python fan |
| `lab/room1/fan/state` | Plain text ON or OFF | Python fan | Node-RED dashboard |

Python on the laptop connects to `127.0.0.1:1883`. Node-RED inside Docker uses
`mosquitto:1883`. The flow already has the correct container-side address.

## Install and start

If this project is already running, retain your existing Compose services and
Python environment. Import the updated flow using the upgrade steps below.

For a fresh setup, extract `mqtt-software-lab.zip` into `~/iot-lab`, then:

```bash
cd ~/iot-lab/mqtt-software-lab
docker --version
docker compose version
sudo docker compose up -d
sudo docker compose ps
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

If venv or unzip is missing, install `python3-venv` and `unzip` using apt.
If Docker/Compose is missing, follow the Docker installation guide linked below.
When using the Ubuntu repository on Linux Mint, use the Ubuntu base codename
from `/etc/os-release`, not the Mint codename. Keep an existing working Docker
installation rather than replacing it just for this project.

Compose exposes MQTT and Node-RED only on the laptop's loopback address. The
MQTT broker allows anonymous connections for this local learning setup. Add
authentication before extending access to other devices.

## Install the dashboard nodes once

1. Open http://127.0.0.1:1880.
2. Open the top-right menu > Manage palette > Install.
3. Search for and install **@flowfuse/node-red-dashboard**.
4. Wait for installation to finish. Restart Node-RED if requested.

The dashboard package is installed in the Node-RED data volume and persists
when the container is recreated. Record the installed package version in your
experiment notes. The flow uses FlowFuse Dashboard nodes with hyphenated names
such as `ui-text` and `ui-switch`.

## Upgrade the previous flow

1. Download the updated ZIP and extract its `node-red-flow.json`. The Python
   scripts and Compose settings retain their existing roles. Copy
   `capture_baseline.sh` and `CAPTURE_BASELINE.md` into your existing project
   folder if you want to use the capture helper there.
2. In the Node-RED editor, export your old **Room 1 software lab** flow as a
   backup if you want to keep a local copy.
3. Double-click that old flow tab, mark it Disabled, and Deploy. Its old
   automatic controller must stop sending commands before using the new flow.
4. Menu > Import > select a file > choose the updated `node-red-flow.json`.
5. Deploy the new **Room 1 dashboard lab** flow. Import it only once.
6. Confirm its MQTT nodes show connected.

A fresh setup can skip steps 2-3 and import the dashboard flow directly.

## Start the two simulated devices

Terminal A:

```bash
cd ~/iot-lab/mqtt-software-lab
source .venv/bin/activate
python fan_subscriber.py
```

Wait for the Ready subscription message. Terminal B:

```bash
cd ~/iot-lab/mqtt-software-lab
source .venv/bin/activate
python temperature_publisher.py
```

The publisher sends 24, 25, 27, 29, 30, 28, 26, 25 C at five-second intervals,
then repeats. Every reading has a device ID, sequence number, UTC timestamp,
numeric temperature, and unit. The fan subscriber accepts ON/OFF commands,
prints them, and publishes its reported state.

## Open and use the dashboard

Open http://127.0.0.1:1880/dashboard/room1.

| Widget | Purpose |
|---|---|
| Temperature | Shows the latest received reading, e.g. 24.0 C |
| Reported fan state | Shows the latest state received from the Python fan |
| Control mode | MANUAL or AUTO |
| Fan ON / OFF | Sends an ON or OFF command in manual mode |
| Automatic control | Enables or disables the temperature-based controller |

The flow starts in **manual mode** after deployment. Leave Automatic control
OFF while using the Fan ON / OFF switch. The sensor continues publishing and
the value continues updating while you control the fan manually.

Turn Automatic control ON to enable the previous temperature-based behavior:

| Temperature | Automatic command |
|---|---|
| At least 28 C | ON |
| At most 26 C | OFF |
| Between 26 and 28 C | Keep the previous reported/requested state |

The manual fan switch becomes disabled in automatic mode. Turn Automatic
control OFF to regain manual control. The mode choice stays in memory during
that run; deployment/restart initializes the flow to manual mode again.

Reported fan-state messages update the switch through its input, with
passthrough disabled. This prevents an incoming state report from creating a
new command and a feedback loop. The separate state text shows the fan's
confirmation rather than only the user's requested state.

The switch is initialized from the last known state, falling back to OFF. This
is not a connectivity indicator. Start the Python fan and verify its Ready
message before relying on the displayed state. The temperature display is also
the last received value and does not include an offline/stale-value alarm yet.

## Record baseline.pcapng

See **CAPTURE_BASELINE.md** for the complete five-minute procedure, GUI-only
Wireshark steps, packet filters, expected messages, and session notes.

For a broker-side capture using Wireshark's dumpcap engine, install Wireshark
on the laptop if needed, start Compose, and run:

```bash
cd ~/iot-lab/mqtt-software-lab
bash capture_baseline.sh 300 baseline.pcapng
```

Start the Python programs after recording begins. Use the dashboard normally.
The helper stops after 300 seconds, writes a real pcapng file, and does not
overwrite an existing file. Open the result with:

```bash
wireshark baseline.pcapng
```

The ZIP does not include baseline.pcapng: that file must come from your actual
running session on your laptop.

## Troubleshooting

- **Port 1883 already occupied:** create `.env` beside `compose.yaml` with
  `MQTT_PORT=1884`, then start Compose. Add `--port 1884` to both Python
  commands. Node-RED and the capture helper still use broker-side port 1883.
- **Port 1880 occupied:** set `NODE_RED_PORT=1881` in `.env`; use 1881 in both
  the editor and dashboard URLs.
- **Unknown ui-text/ui-switch nodes:** install `@flowfuse/node-red-dashboard`
  in this Node-RED instance, then reload the editor and Deploy.
- **Dashboard 404:** Deploy and verify ui-base path `/dashboard`, ui-page path
  `/room1`, and dashboard package installation.
- **MQTT disconnected:** broker server must be `mosquitto`, port 1883, no TLS,
  and no username/password for this starter configuration.
- **Manual command immediately reversed:** disable the original flow tab and
  turn Automatic control OFF in the new dashboard. Only the new controller
  should be active.
- **Temperature or fan state absent:** run both Python programs and verify
  their broker port matches the laptop-side published port. Check the Debug
  sidebar and `sudo docker compose logs --tail=50`.

## Shut down

Stop the Python programs with Ctrl+C, then:

```bash
sudo docker compose down
```

This preserves the Node-RED data volume, installed dashboard package, and flow.
Adding `-v` would remove named volumes, so use it only for an intentional reset.

## Validation and limits

The original Python scripts and basic Node-RED message loop were tested together
in the initial project version using Node-RED 4.1.15 and a temporary MQTT
broker. This update checks flow JSON, all node references, function syntax,
manual/automatic gating, threshold decisions, and state-feedback behavior.
The new dashboard has not been run in a live Node-RED browser session here;
install, deploy, and verify it on your laptop before recording. The capture
helper is syntax-checked; it must run against your Linux Docker broker to
produce the actual baseline.

## Official references

- Dashboard installation: https://dashboard.flowfuse.com/getting-started.html
- Text widget: https://dashboard.flowfuse.com/nodes/widgets/ui-text.html
- Switch widget: https://dashboard.flowfuse.com/nodes/widgets/ui-switch.html
- Docker/Node-RED: https://nodered.org/docs/getting-started/docker
- Docker installation: https://docs.docker.com/engine/install/ubuntu/
- Paho MQTT client: https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html

Python dependency is pinned to paho-mqtt 2.1.0 and Node-RED to image 4.1.15.
Mosquitto uses the moving major-version image tag `2`; record actual image
digests and the dashboard package version for reproducible experiments.
