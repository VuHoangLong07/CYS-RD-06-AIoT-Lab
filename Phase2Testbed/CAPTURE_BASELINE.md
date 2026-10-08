# Record a real normal session as baseline-distance-simulated.pcapng

Run this on your Linux laptop running rootful Docker. Use this page for S0;
use evidence-and-comparison.md for S1/S2 clients and filenames.
The project ZIP does not contain a fabricated packet capture. `baseline-distance-simulated.pcapng`
will be created from the traffic your running application actually generates.

## Prepare Wireshark

If needed, install the tools:

```bash
sudo apt update
sudo apt install wireshark wireshark-common
```

For capture from the GUI as your normal user, configure permissions:

```bash
sudo dpkg-reconfigure wireshark-common
sudo usermod -aG wireshark "$USER"
```

Answer Yes to allowing non-superusers to capture. Log out and log back in so
the group membership takes effect. Run the Wireshark GUI as your normal user.
The broker-side helper below uses sudo for capture and does not require this
group setup.

## Recommended capture point: the broker

The Python programs connect from the laptop, while Node-RED connects from
another Docker container. Capturing just the Wi-Fi interface can miss all this
traffic. Capturing only loopback may miss the Node-RED-to-broker connection.

The helper uses Wireshark's `dumpcap` capture engine inside the broker's Linux
network namespace, on `eth0`. It captures traffic reaching/leaving Mosquitto,
including Python clients, physical UNO clients and Node-RED, at one observation point. It avoids
collecting the same forwarded packet at several host interfaces.

The namespace command does not install anything inside Mosquitto. It uses the
laptop's existing dumpcap binary. This method assumes the rootful Linux Docker
setup used by this project's `sudo docker` commands, and a broker `eth0`
interface.

## Five-minute normal-session procedure

1. Start Compose and deploy the dashboard flow. Confirm it works before
   recording. Finish package installations and troubleshooting first.
2. Stop the Python programs with Ctrl+C, leaving Compose running.
3. In Terminal C, enter the project folder and start the capture:

   ```bash
   cd ~/iot-lab/mqtt-distance-testbed
   bash capture_baseline.sh 300 baseline-distance-simulated.pcapng
   ```

4. Wait for dumpcap to say it is capturing on `eth0`. In Terminal A, start the
   fan subscriber, and in Terminal B, start the default publisher:

   ```bash
   cd ~/iot-lab/mqtt-distance-testbed
   source .venv/bin/activate
   python fan_subscriber.py
   ```

   ```bash
   cd ~/iot-lab/mqtt-distance-testbed
   source .venv/bin/activate
   python distance_publisher.py
   ```

   Add `--port 1884` to each Python command if `.env` sets `MQTT_PORT=1884`.
   The capture helper still filters broker-side port 1883.

5. Open http://127.0.0.1:1880/distance-lab/room1. Use this schedule and note the
   actual action times:

   | Approximate time after capture starts | Normal activity |
   |---|---|
   | 0-60 s | Start clients; keep Automatic control OFF and watch distances |
   | 60 s | Turn the manual LED ON / OFF switch ON |
   | 90 s | Turn the switch OFF |
   | 120 s | Enable Automatic control |
   | 120-300 s | Let the normal five-second readings and threshold decisions run |

6. At 300 seconds, dumpcap stops automatically and saves `baseline-distance-simulated.pcapng` in
   the project folder. Stop the Python programs afterwards. For a baseline
   that includes client disconnections, stop the Python programs just before
   capture ends instead and record that choice.
7. Open the capture as your normal user:

   ```bash
   wireshark baseline-distance-simulated.pcapng
   ```

The helper refuses to overwrite an existing file. For another run, select a
different filename, for example `baseline-02.pcapng`. It writes pcapng using
dumpcap's default file format. Do not rename a tcpdump `.pcap` file to `.pcapng`
and assume the format changed.

## GUI-only Wireshark alternative

1. Open Wireshark as your normal user.
2. Open Capture > Options and select **any**. This covers Docker interfaces
   as well as loopback. Do not select only `wlo1` for this local Docker lab.
3. For the unchanged starter project, set the **capture filter** to:

   ```text
   tcp port 1883
   ```

   If the laptop uses port 1884, use:

   ```text
   tcp port 1883 or tcp port 1884
   ```

4. Start recording, then start the Python programs and follow the schedule
   above. Stop after five minutes using the red Stop button.
5. File > Save As, choose **pcapng**, and save as
   `~/iot-lab/mqtt-distance-testbed/baseline-distance-simulated.pcapng`.
6. Save **all captured packets**, not just the currently displayed packets.

An `any` capture can contain repeated observations of a packet crossing Docker
interfaces and traffic from another local broker using these ports. It is
useful for inspecting the application; use the single broker-side observation
point above when you need packet counts suitable for repeatable IDS analysis.

## Inspect and verify the saved file

Use the Wireshark **display filter**:

```text
mqtt
```

For MQTT PUBLISH messages only:

```text
mqtt.msgtype == 3
```

For the simulated distance topic:

```text
mqtt.topic == "lab/room1/distance"
```

Expand MQTT in the packet details. Look for the topic name and message payload.
You should see the three topics:

- `lab/room1/distance`: JSON readings roughly five seconds apart.
- `lab/room1/fan/set`: ON/OFF commands during manual or automatic control.
- `lab/room1/fan/state`: the virtual fan's reported state.

CONNECT/SUBSCRIBE packets appear when clients start; QoS 1 PUBLISH/PUBACK
exchanges are expected. TCP acknowledgment packets and retained state messages
are normal. One MQTT message is not necessarily one TCP packet: messages can
be split across TCP packets or combined in one packet.

If port 1884 is not recognized as MQTT, select a relevant TCP packet, use
Analyze > Decode As, and choose MQTT for that port. The broker-side capture
uses 1883 and normally decodes automatically.

Optional verification if `capinfos` is installed:

```bash
capinfos baseline-distance-simulated.pcapng
```

Check the file format is pcapng, the packet count is nonzero, and the duration
and timestamps are consistent with your session. Capture duration reflects the
first and last recorded packets; it can be shorter than the 300-second timer.

## Record session notes

Beside the capture, keep a short `baseline-notes.txt` with date, duration,
capture point, filter, MQTT port, publisher interval, dashboard package version,
Auto/Manual schedule, actual action times, and any unexpected reconnections.
This S0 baseline represents the simulated distance workload. Record S1/S2
separately with the same broker-side observation point. Preserve the original
temperature baseline.pcapng.

## Official references

- Capture filters: https://www.wireshark.org/docs/wsug_html_chunked/ChCapCaptureFilterSection.html
- Saving captured packets: https://www.wireshark.org/docs/wsug_html_chunked/ChIOSaveSection.html
- Wireshark dumpcap: https://www.wireshark.org/docs/man-pages/dumpcap.html
