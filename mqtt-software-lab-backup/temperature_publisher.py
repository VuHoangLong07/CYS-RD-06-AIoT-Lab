#!/usr/bin/env python3
"""Replace a temperature sensor with a repeatable, software-only MQTT source."""

import argparse
import json
import math
import sys
import threading
import time
import uuid
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

TOPIC = "lab/room1/temperature"
TEMPERATURES = [24.0, 25.0, 27.0, 29.0, 30.0, 28.0, 26.0, 25.0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--interval", type=float, default=5.0)
    parser.add_argument("--value", type=float, help="Publish one fixed temperature repeatedly")
    parser.add_argument("--count", type=int, default=0, help="Stop after N readings; 0 means forever")
    args = parser.parse_args()
    if not math.isfinite(args.interval) or args.interval <= 0 or args.count < 0:
        parser.error("--interval must be a finite positive number; --count must be nonnegative")
    if args.value is not None and not math.isfinite(args.value):
        parser.error("--value must be finite")

    connected = threading.Event()
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="temperature-" + uuid.uuid4().hex[:8],
        protocol=mqtt.MQTTv311,
    )

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"Connection rejected: {reason_code}", flush=True)
            return
        connected.set()
        print(f"Connected to {args.host}:{args.port}", flush=True)

    def on_disconnect(client, userdata, flags, reason_code, properties):
        connected.clear()
        if reason_code.is_failure:
            print("Broker disconnected; the MQTT client will try to reconnect.", flush=True)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    try:
        # connect opens the socket; the network loop processes MQTT acknowledgments.
        client.connect(args.host, args.port, keepalive=60)
        client.loop_start()
        if not connected.wait(10):
            raise TimeoutError("No successful MQTT connection within 10 seconds")

        seq = 0
        while args.count == 0 or seq < args.count:
            if not connected.wait(10):
                raise TimeoutError("Broker did not reconnect within 10 seconds")
            temperature = args.value if args.value is not None else TEMPERATURES[seq % len(TEMPERATURES)]
            seq += 1
            payload = json.dumps({
                "device": "sim-room1",
                "seq": seq,
                "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
                "temperature": temperature,
                "unit": "C",
            })
            # QoS 1 waits for the broker's acknowledgment. Telemetry is not retained.
            result = client.publish(TOPIC, payload, qos=1, retain=False)
            result.wait_for_publish(timeout=10)
            if not result.is_published():
                raise TimeoutError("Broker did not acknowledge the temperature reading")
            print(f"SENT {TOPIC} {payload}", flush=True)
            if args.count and seq >= args.count:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nTemperature publisher stopped.", flush=True)
    except (OSError, RuntimeError, TimeoutError, ValueError) as exc:
        print(f"MQTT error: {exc}. Check docker compose ps and your broker port.", file=sys.stderr)
        return 1
    finally:
        client.disconnect()
        client.loop_stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
