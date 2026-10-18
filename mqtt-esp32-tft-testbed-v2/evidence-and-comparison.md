# Phase 2 evidence and comparison

## Roles and stages

| Stage | Telemetry publisher | Command subscriber / state producer |
|---|---|---|
| S0 | Python simulator | Python virtual fan |
| S1 | ESP32, ENABLE_SUBSCRIBER=false | Python virtual fan |
| S2 | ESP32, both flags true | Same ESP32 and TFT indication |

S2 is the delivered default. If time does not allow S1, compare S0 and S2 and
state that both roles changed together. Do not claim a staged single-role test
you did not perform. Disconnect previous UNO boards and stop old simulators/
controllers so they do not share the command/state topics during the experiment.

Use the same broker-side eth0 observation point, port/filter, 300 s duration,
v2 timing profile, seed, base intervals, run duration and 28/26 C thresholds
in each comparison. Record actual intervals rather than assuming fixed spacing. The older
lab/esp32/telemetry capture may have used QoS 0 and a 2 s interval: record those
differences rather than claiming identical conditions. This rebuilt comparison
uses QoS 1, a shared room/scheduling model and a 5 s environmental base interval.
The physical model includes a modest response to the local logical fan state;
the software publisher does not read the virtual fan state. ESP32 diagnostics are
real only on the hardware node; Python uses null for those fields. Identical seeds
make controlled model tests reproducible; actual packet timing also depends on
network, acknowledgment and loop delays, and the stages are not byte-identical.

## Normal-session schedule

| Capture-relative time | Action |
|---:|---|
| 0 s | Start capture, then clients / ESP32 when practical; record actual times |
| 0-60 s | Keep dashboard in MANUAL and watch publications |
| 60 s | Manual ON |
| 90 s | Manual OFF |
| 120 s | Enable Automatic control |
| 120-300 s | Let the slowly changing room model trigger normal ON/OFF commands |
| 300 s | Capture stops; save notes and screenshots |

Use one filename per stage:

```bash
bash capture_baseline.sh 300 baseline-esp32-simulated.pcapng
bash capture_baseline.sh 300 baseline-esp32-publisher.pcapng
bash capture_baseline.sh 300 baseline-esp32-pubsub.pcapng
```

Run each command in its matching stage, not all at once. Preserve earlier
baseline.pcapng/distance capture files. Start and connection timing can vary;
record actual timestamps and use capture times for analysis.

## Compare MQTT messages and directions

Useful Wireshark display filters:

```text
mqtt
mqtt.msgtype == 3
mqtt.topic == "lab/esp32/telemetry"
mqtt.topic == "lab/room1/fan/set"
mqtt.topic == "lab/room1/fan/state"
tcp.analysis.retransmission
```

| Property | What to measure / interpret |
|---|---|
| Identities / streams | Client IDs and endpoint IPs; map the ESP32 to inventory MAC/IP |
| Telemetry | Source-to-broker seq, reading count, interval and payload size |
| PUB status | Compare TFT OK count / Serial PUB_ACK with telemetry PUBLISH/PUBACK |
| SUB status | SUBSCRIBE/SUBACK, incoming ON/OFF, accepted RX count and state feedback |
| Combined roles | S2 puts telemetry, command reception and state feedback on one ESP32 TCP stream; S0 uses two role-client connections |
| Latency | Broker-observed command followed by state PUBLISH, for one command at a time |
| Reconnects / retries | Wi-Fi variation, connection changes and PUB failures; no invented performance values |
| Payload source | Environmental fields are simulated in both; physical node adds real RSSI/heap/uptime/runtime diagnostics |
| Stream rates | Separate environment, metrics and event topics; report distributions and burst/gap counts |
| Configuration | Record v2 schema, firmware, model seed, profile and boot ID; preserve old v1 captures separately |
| Extra messages | ONLINE/OFFLINE Last Will status and retained initial fan state are intentional |

The TFT's PUB OK counter counts successful telemetry acknowledgments only;
feedback/status publications also appear in the capture. RX counts accepted
deliveries, including repeated valid ON/OFF. QoS 1 permits duplicate delivery;
idempotent commands keep the same state, but counters/feedback may increase.

One source publish can be forwarded to several subscribers. Count the
source-to-broker topic/direction rather than every matching forwarded copy.
MQTT message count and TCP packet count differ: messages can be split across
packets or combined. In S2, one TCP stream carries both directions and several
topics, so a stream filter alone does not isolate telemetry.

uptime_ms is not synchronized to the laptop clock. Use capture timestamps for
intervals and delay calculations. Command-to-state delay includes broker and
network handling, not the TFT's exact pixel-update time. The simple ON/OFF
protocol has no command IDs, so overlapping commands cannot be correlated
unambiguously. Screen refresh and network operations also add visible delay.

## Results to fill with measurements

| Metric | S0 | S1 | S2 |
|---|---|---|---|
| Run date / operator / ID | TBD | TBD | TBD |
| Duration / observation point / filter | TBD | TBD | TBD |
| Source telemetry count | TBD | TBD | TBD |
| Median / min / max environmental source interval | TBD | TBD | TBD |
| Metrics / occupancy-event source counts | TBD | TBD | TBD |
| Timing profile / model seed / boot ID | TBD | TBD | TBD |
| Median payload bytes | TBD | TBD | TBD |
| Telemetry PUBACK / publish failures | TBD | TBD | TBD |
| Command and state counts | TBD | TBD | TBD |
| Manual command response observations | TBD | TBD | TBD |
| Role-client connection count | TBD | TBD | TBD |
| Reconnect / retransmission notes | TBD | TBD | TBD |

## Required evidence

1. Wiring diagram plus real photographs of the ESP32/TFT pin labels and wires.
2. Tested firmware, display settings, version strings, library/core versions,
   source hashes and a redacted lab_config.h.
3. Completed device inventory with actual MAC/IP, role flags and dates.
4. Captures and run notes with duration, point/filter, interval, thresholds,
   actual actions and any errors/reconnects.
5. A short video showing PUB OK increasing while manual commands raise SUB RX
   and change FAN state; show matching broker/topic/state output and automatic
   transitions if used. Keep credentials out of the video.
6. Optional device-check.json from `python verify_device.py --output device-check.json`.
   This contains actual MQTT observations when run on your laptop with only the
   ESP32 active. It supplements the real TFT video; it does not observe the screen.

Optional broker-disconnect or invalid-payload tests belong in separate
recordings. A logical TFT indication is the demonstrated output; do not label
it a tested relay or a physical cooling fan.
