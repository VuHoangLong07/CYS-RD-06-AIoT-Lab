# Evidence and simulated/physical traffic comparison

## Stages

| Stage | Distance source | Actuator | Capture filename |
|---|---|---|---|
| S0 | distance_publisher.py | fan_subscriber.py | baseline-distance-simulated.pcapng |
| S1 | UNO #1 + ABX00102 | fan_subscriber.py | baseline-distance-sensor.pcapng |
| S2 | UNO #1 + ABX00102 | UNO #2 + LED | baseline-distance-physical.pcapng |

Use one source per role. Unpower board #2 during S1. Stop the Python sensor
before S1 and the Python actuator before S2. Disable earlier controller flows.
Keep the original temperature baseline.pcapng unchanged; make a new distance
software baseline for a comparison with matching schema/units/control logic.

## Repeated normal-session capture

Install Wireshark/dumpcap as described in CAPTURE_BASELINE.md. Use the helper
at the broker's eth0 for all three stages: one observation point, 300 s each,
broker-side capture filter tcp port 1883. The output is a genuine pcapng file
from Wireshark's capture engine. The helper refuses an existing output filename.

For each run, set Automatic control OFF before starting. Start capture first,
then the appropriate source and actuator when practical. Record their actual
start times. Follow this schedule relative to capture start:

| Time | Action |
|---:|---|
| 0 s | Start capture; start stage clients / power boards |
| 60 s | Manual ON |
| 90 s | Manual OFF |
| 120 s | Enable Automatic control |
| 150 s | For physical source, hold opaque target about 150 mm away |
| 180 s | Move target to about 400 mm |
| 210 s | Move target to about 150 mm |
| 240 s | Move target to about 400 mm |
| 300 s | Capture stops; save notes/screenshots |

Keep each target position stable until the next movement. Do not perform
installations, reboot troubleshooting or deliberate fault tests during the
normal baseline. Record any deviation. A scripted simulator and human-moved
target do not create identical command/value sequences: compare the rate and
protocol first, and document the intentional input differences.

Run each command in its matching stage:

```bash
bash capture_baseline.sh 300 baseline-distance-simulated.pcapng
bash capture_baseline.sh 300 baseline-distance-sensor.pcapng
bash capture_baseline.sh 300 baseline-distance-physical.pcapng
```

Open each with Wireshark. No Wi-Fi monitor-mode adapter is needed for this
broker-side MQTT comparison; this capture does not include raw 802.11 radio
frames or traffic that never reached the laptop/broker.

## Useful Wireshark display filters

```text
mqtt
mqtt.msgtype == 3
mqtt.topic == "lab/room1/distance"
mqtt.topic == "lab/room1/fan/set"
mqtt.topic == "lab/room1/fan/state"
tcp.analysis.retransmission
```

## What to compare

| Property | Record / interpretation |
|---|---|
| Capture conditions | Duration, point/filter, client starts, manual action times, thresholds and 5 s configured rate |
| Identity | MQTT client IDs and TCP conversations; connect the hardware IDs to inventory MAC/IP |
| Source reading count | Source-to-broker distance PUBLISH messages and seq values |
| Reading interval | Median/min/max time between source readings using packet timestamps |
| Payload | Numeric distance_mm, unit mm, valid true, firmware/device IDs and payload bytes |
| QoS / retain | QoS 1, PUBACK, DUP; telemetry/command nonretained, output/status retained |
| Connections | CONNECT, SUBSCRIBE, keepalives and reconnects |
| Commands / state | Manual ON/OFF and following actuator state report; broker ACK alone does not confirm actuation |
| Retransmissions | Counts per TCP stream; normal network variation can cause retransmissions |
| Extra physical messages | ONLINE/OFFLINE sensor and actuator status/Last Will topics |

Both distance sources use the same field names. Different device/version
strings and Python JSON spacing can change payload length. The hardware reports
uptime_ms since board boot; Python reports elapsed process time after connection.
These are not synchronized clocks. Use Wireshark packet times for comparisons.

One source publish can be forwarded to multiple subscribers. Counting every
packet with a topic counts forwarded copies as well as the original reading.
Select the source-to-broker conversation for reading-rate measurement. TCP
packet count is not MQTT message count: multiple messages can share a packet,
and one message can span packets. QoS 1 allows duplicates; inspect seq/DUP.

For manual commands sent one at a time, compare the controller-to-broker
command with the following actuator-to-broker state message. This gives a
broker-observed response delay, including networking and broker processing.
It does not measure the LED's exact electrical switching time. ON/OFF payloads
have no command ID, so overlapping requests cannot be correlated unambiguously.

Sensor MQTT status indicates broker connectivity, not measurement health.
The physical sensor can omit invalid reads while remaining ONLINE. Retained
state/value displays are last known and can persist after unplugging a board.

## Fill with measured results

| Metric | S0 | S1 | S2 |
|---|---|---|---|
| Date / operator / run ID | TBD | TBD | TBD |
| Actual capture duration | TBD | TBD | TBD |
| Source reading count | TBD | TBD | TBD |
| Reading interval median/min/max | TBD | TBD | TBD |
| Median payload bytes | TBD | TBD | TBD |
| Client reconnect count | TBD | TBD | TBD |
| ON/OFF state response observations | TBD | TBD | TBD |
| Retransmission notes | TBD | TBD | TBD |
| Invalid/omitted physical readings | Not applicable | TBD | TBD |

## Submission evidence

1. Wiring-diagram.png/svg plus photographs of the real boards, cable and LED.
2. Firmware source, redacted lab_config.h, exact board/library/firmware versions
   and hashes of the tested source.
3. Completed device-inventory.md with real labels, MAC/IP, model and dates.
4. The three real captures plus run notes with point/filter/duration, source,
   versions, thresholds, action times, errors and reconnects.
5. A short demonstration video showing the labelled boards and dashboard:
   manual ON/OFF; near target -> distance changes -> automatic LED ON; far
   target -> LED OFF; show reported state and Serial identity. Keep credentials
   out of the video and source submitted for review.

Optional connection-loss tests belong in a separate recording with separate
notes; do not call them normal baseline behavior.
