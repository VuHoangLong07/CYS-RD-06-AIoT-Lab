# Capture a normal ESP32 PUB/SUB session

Use this on the Linux Mint laptop hosting rootful Docker. Install tools if needed:

```bash
sudo apt update
sudo apt install wireshark wireshark-common mosquitto-clients
```

The included helper uses sudo for broker-side capture. The Wireshark GUI should
run as your normal user. For GUI capture permissions, use dpkg-reconfigure
wireshark-common and add your user to the wireshark group, then logout/login.

Run from the directory containing this project's compose.yaml:

```bash
bash capture_baseline.sh 300 baseline-esp32-pubsub.pcapng
```

The helper locates the running Mosquitto container, enters only its network
namespace and uses the laptop's dumpcap on eth0 with capture filter
`tcp port 1883`. That includes Docker controller traffic, Python clients and
the physical ESP32 at one observation point. It avoids duplicated observations
across the laptop's several Docker interfaces. It assumes rootful Linux Docker
and a broker eth0 interface; it is not a Docker Desktop/Windows capture recipe.

Container-side port remains 1883 even when the host maps it to 1884. Output is
dumpcap's native pcapng format. The helper refuses an existing filename and
restores ownership so your normal user can open the result.

Complete installations/troubleshooting before recording a normal session. Set
dashboard Automatic control OFF, start capture, then start the selected clients
or power the ESP32. Follow evidence-and-comparison.md's timed actions. Use a
matching filename for each stage; keep previous baseline.pcapng files.

```bash
wireshark baseline-esp32-pubsub.pcapng
capinfos baseline-esp32-pubsub.pcapng
```

Use Wireshark display filter `mqtt`. The capture filter and display filter are
different syntaxes. Expect CONNECT, SUBSCRIBE/SUBACK, PUBLISH/PUBACK and normal
TCP acknowledgments. Select a source-to-broker conversation/topic when counting
readings; forwarded publishes are additional copies. A TCP packet is not
necessarily one MQTT message.

For a GUI-only alternative, capture the host's any interface with
`tcp port 1883` (or `tcp port 1883 or tcp port 1884` for that host mapping), stop
after five minutes and Save As pcapng with all packets. Record that observation
point: any can include multiple observations of one packet and should not be
mixed with broker-side captures for direct packet-count comparisons.

Do not include deliberate disconnects or invalid-command tests in a normal
baseline. Capture those separately and label them. The package does not include
an invented capture or claim you have already performed the physical test.

References: https://www.wireshark.org/docs/man-pages/dumpcap.html ;
https://www.wireshark.org/docs/wsug_html_chunked/ChCapCaptureFilterSection.html
