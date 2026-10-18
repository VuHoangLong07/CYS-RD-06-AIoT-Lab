# Version 2 validation report

Date: 2026-10-18. Target: classic ESP32 Dev Module with the existing ST7789 TFT.

## Completed checks

- The combined sketch compiled as native C++17 against temporary Arduino,
  Wi-Fi, ArduinoMqttClient and TFT API mocks, with warnings/format checks enabled.
  Both ESP32 core-major 2 and 3 conditional paths passed. This is not an ESP32
  target build or a test of the actual library implementations.
- Sketch tests exercised exact ON/OFF and rejection of lowercase, whitespace,
  embedded-NUL, oversized, wrong-topic and retained-replay commands. Commands
  arriving during telemetry/state PUBACK waits did not cause nested publication;
  a newer state remained pending until reported.
- Failed environment PUBACK did not increment success. Detected connection loss
  set the logical state OFF. Reconnection resubscribed and staggered future
  reports without replaying missed environment/occupancy publications.
- Fair background selection serviced all three streams when they were due.
  Timers passed a uint32 rollover check. Screen writes stayed in 240x240 bounds
  with large counters. Physical TFT SPI signalling is not tested by mocks.
- A ten-minute mocked sketch run produced 134 environment messages, 40 metrics
  messages and 10 occupancy events. Environmental intervals ranged 900-9400 ms
  in that fixture, with a mean of approximately 4485.7 ms. These are desktop
  fixture observations, not performance measurements or predicted guarantees
  for the user's hardware. JSON parsed; environmental values were bounded;
  numeric/boolean types and explicit simulated-field labels were present.
- C++/Python controlled model runs matched 133 environment publication times
  exactly over ten simulated minutes; environmental values agreed within 0.02.
  This isolated comparison excluded networking and the logical fan's influence.
- Six dependency-free Python unit tests checked seed reproducibility, one-hour
  model bounds, jitter/gap limits, bounded event follow-ups, fixed scheduling,
  reconnect deadlines and schema/source labels. Native C++ model assertions passed.
- The software publisher passed mocked fixed/variable sessions: three topics,
  QoS 1/nonretained messages, increasing per-stream counters, null ESP32-only
  measurements, proper shutdown and failure on missing publish acknowledgment.
- Node-RED functions passed rich-field, null-metric and occupancy-event formatting,
  manual guard and 28/26 C hysteresis checks. Node IDs, wires and configuration
  references were validated. No live Node-RED UI was available here.
- Capture helper now sends dumpcap bytes to stdout and lets the calling shell
  create the output file, incorporating the earlier output-permission fix.

## Required checks on the laptop

Arduino target compilation tools, Docker, the ESP32 and TFT are unavailable in
this workspace. Compile/upload with the actual ESP32 core and libraries; verify
screen/network behaviour and the dashboard; test the commands; record versions
and actual MAC/IP/boot ID; capture a new normal session. No included artifact is
claimed to be a physical demonstration or genuine packet capture.

Synthetic room values and scheduling are testbed models, not measurements from
physical environmental sensors or a calibrated model of deployed IoT traffic.
The legacy v1 screen preview remains an illustration. Recalculate the firmware
hashes after local credential/display edits and redact shared credentials.
