# Fedora testbed: startup, capture, and bounded TCP SYN burst

This guide is grounded in the uploaded `testbedsample.zip`, especially
`README_FEDORA.md`, `compose.yaml`, the Python clients, the broker configuration,
`node-red-flow.json`, and `capture_baseline.sh`. 
Most of these instructions were derived from Son's first testbench
Additional modifications might be needed depending on the env you're running on. You're on your own :)

## What actually runs

Docker Compose starts **Mosquitto and Node-RED only**. The Python temperature
publisher and virtual fan run on Fedora, outside Docker.

| Component | Address or role |
| --- | --- |
| Host Python clients | `127.0.0.1:1883` by default |
| Node-RED editor | `http://127.0.0.1:1880` by default |
| Node-RED MQTT connection | `mosquitto:1883` within Docker |
| Capture | Mosquitto network namespace, `eth0`, internal TCP/1883 |
| Burst | Fedora host to the currently discovered Mosquitto Docker IP, TCP/1883 |

The included flow has no dashboard, AUTO/MANUAL mode switch, or extra palette
dependency. Use its Debug sidebar. Its automatic controller runs on every valid
reading: >=28 C requests ON; <=26 C requests OFF; between thresholds it keeps
its last requested state. Every valid reading sends a fan command.

## 1. Install the kit into your deployed project

Download the zip (if you're getting this from the repo then download all files present below). Remember to add this to the same folder you downloaded Son's simulated testbench

If you moved the deployed project, substitute its actual folder. Install the
kit alongside the existing `compose.yaml`; do not launch another copy of this
Compose project. The kit contains:

- `capture_baseline.sh`: Capture script
- `run_tcp_burst.sh`: Main script to run attack simulation
- `README.md`: This file duh
- `ATKSim01.odt`: Short report
- `run_tcp_burst_explained.txt`: A detailed, AI generated explanation of the main bash script for anyone wanting to learn the details of the operation itself
No executable-bit change is necessary when invoking the scripts with `bash`.

## 2. Start Docker and check dependencies

Keep your existing rootful Docker Engine installation. Do not reinstall it.

```bash
sudo systemctl start docker
sudo docker --version
sudo docker compose version
sudo dnf install nmap wireshark-cli util-linux python3 python3-pip unzip iproute
```

The capture still uses `dumpcap` through the shell helper. `tshark` is used only
to export and check the resulting capture. This workflow does not use tcpdump.

## 3. Start Mosquitto and Node-RED

In the project folder:

```bash
sudo docker compose config
sudo docker compose up -d
sudo docker compose ps
sudo docker compose logs --tail=50
```

Both services should be running. `depends_on` is not a readiness check; wait
for Node-RED's server to start and its MQTT nodes to show connected.

The uploaded Compose defaults are loopback-bound host ports 1883 and 1880.
If your existing `.env` or environment overrides `MQTT_PORT` or `NODE_RED_PORT`,
use the mappings shown by `compose ps`. Do not change them for this experiment.

## 4. Check Python and the deployed Node-RED flow

For an existing working environment:

```bash
source .venv/bin/activate
python -c 'import paho.mqtt.client; print("Paho MQTT available")'
```

If using a freshly extracted project, the archived `.venv` is not a portable
installation. Create a new environment instead:

```bash
python3 -m venv .venv-tcp-test
source .venv-tcp-test/bin/activate
python -m pip install -r requirements.txt
```

Use that environment's activation path in the client terminals below if needed.

Open **http://127.0.0.1:1880** (or your actual published editor port). Check that
the **Room 1 software lab** flow is deployed and the MQTT nodes show connected.
If already deployed, reuse it. Otherwise Menu -> Import -> select the existing
`node-red-flow.json` -> Import -> Deploy. Open the Debug sidebar. The broker
setting inside the flow must be `mosquitto`, port `1883`.

## 5. Start the virtual fan — Terminal A

```bash
source .venv/bin/activate
python -u fan_subscriber.py
```

Wait for `Ready: subscribed to lab/room1/fan/set`. Before starting the sensor,
click **Manual fan ON**, then **Manual fan OFF** in Node-RED and confirm that
this terminal prints the corresponding received commands.

If your MQTT host port is 1884, use `python -u fan_subscriber.py --port 1884`.

## 6. Start the temperature publisher — Terminal B

```bash
source .venv/bin/activate
python -u temperature_subscriber.py
```

If your MQTT host port is 1884, add `--port 1884`.

Confirm readings reach Node-RED and fan commands arrive in Terminal A. The
sequence is `24, 25, 27, 29, 30, 28, 26, 25`, repeating with a nominal 5-second
interval. The real interval also includes processing and acknowledgement time.
Leave both clients running throughout the experiment. Do not click manual fan
buttons during the measured run.

## 7. Run the experiment — Terminal C

```bash
bash run_tcp_burst.sh
```

Run as your regular user; the script requests sudo for the operations that need
it. Do not separately start another capture helper for this run. The runner
calls `capture_baseline.sh` itself, after checking the broker's direct Docker IP.

The SYN burst uses the container's Docker IP so it enters the same broker
`eth0` that the capture helper observes. Think of the capture as a camera at
the broker's doorway: the test sends traffic through that doorway. Normal
Python clients keep using the published localhost port. Even when the host
MQTT port is changed to 1884, the burst and capture still use internal 1883.

Do not recreate/restart Mosquitto during capture: that changes its network
namespace. Ctrl+C interrupts the runner; incomplete runs are marked and capture
logs identify any retained partial capture. Rerunning creates a new results
folder rather than overwriting the previous run.

## 8. Inspect the evidence

The script prints a folder such as `results/tcp-syn-20261007T041000-12345/`.
It contains:

| File | Purpose |
| --- | --- |
| `traffic.pcapng` | Original broker-interface capture |
| `traffic.csv` | Packet fields, including the earlier MQTT fields and TCP flags |
| `timeline.csv` | UTC and epoch markers for baseline, burst, recovery, completion |
| `metadata.txt` | Target IP/container, parameters, route, and Nping version |
| `nping.log` | Generator output and packet totals |
| `capture.log` | Capture startup, errors, dropped |
| `syn_count.txt` | Count of SYN packets |

Open `traffic.pcapng` in Wireshark. Display filters:

```text
tcp.dstport == 1883 && tcp.srcport == 40000 && tcp.flags.syn == 1 && tcp.flags.ack == 0
```

For a more exact filter, also add `ip.dst == <broker IP from metadata.txt>`.
The SYN count should normally be close to 1500; investigate differences using
the Nping and capture logs. Other frames include normal MQTT, SYN-ACKs, RSTs,
and acknowledgements; the full capture is not expected to contain exactly
1500 frames.

```text
mqtt
```


## 9. Finish

After the runner finishes, stop each Python client with Ctrl+C. In the project
folder:

```bash
sudo docker compose down
```

Do not use `down -v` for routine shutdown: it deletes saved flows and broker
data. Preserve each run's capture and logs alongside its CSV.

## Troubleshooting

- **Capture cannot write:** inspect `capture.log`. This supplied helper restores
  the working temporary-directory permission fix. Do not disable SELinux or
  apply recursive permission changes to the project.
- **Connection refused:** check `sudo docker compose ps` and logs. Confirm host
  port overrides for Python; the burst requires the rootful Docker bridge route
  from Fedora to the broker container.
- **No matching SYNs:** inspect `metadata.txt`, `nping.log`, and `capture.log`.
  Keep the result as a failed/incomplete traffic-injection run. Do not conclude
  that the broker resisted an attack that never reached it.
- **One client quits during the run:** record its error and timestamp; restarting
  changes the experiment. Start a new complete run once normal operation is
  restored rather than treating it as a clean recovery window.
- **Existing shell scripts still differ:** the uploaded helper did not include
  the later temp-folder change. This kit intentionally installs the corrected
  helper while backing up your deployed copy first.


