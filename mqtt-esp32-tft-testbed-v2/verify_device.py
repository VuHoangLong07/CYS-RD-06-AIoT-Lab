#!/usr/bin/env python3
"""Check live ESP32 telemetry and ON/OFF feedback while publishing continues."""

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import queue
import sys
import time
import uuid

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

TELEMETRY_TOPIC = "lab/esp32/telemetry"
COMMAND_TOPIC = "lab/room1/fan/set"
STATE_TOPIC = "lab/room1/fan/state"


def telemetry(payload, device):
    try:
        data = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict) or data.get("device") != device:
        return None
    temperature = data.get("temperature")
    sequence = data.get("seq")
    if (isinstance(temperature, bool) or not isinstance(temperature, (int, float))
            or not math.isfinite(temperature) or type(sequence) is not int or sequence < 1
            or data.get("sensor") != "simulated" or data.get("unit") != "C"):
        raise ValueError("Expected valid numeric ESP32 telemetry labelled sensor=simulated, unit=C")
    if data.get("schema") == 2:
        for field in ("humidity_pct", "light_lux", "pressure_hpa", "co2_ppm"):
            value = data.get(field)
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise ValueError(f"Schema 2 requires finite numeric {field}")
        fields = {"temperature", "humidity_pct", "light_lux", "pressure_hpa", "co2_ppm", "occupied"}
        declared = data.get("simulated_fields")
        if (type(data.get("occupied")) is not bool or not isinstance(declared, list)
                or any(not isinstance(field, str) for field in declared) or not fields.issubset(declared)):
            raise ValueError("Schema 2 must label its simulated fields and use boolean occupied")
    return data


def wait_event(events, predicate, timeout, label):
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"Timed out waiting for {label}")
        try:
            event = events.get(timeout=remaining)
        except queue.Empty:
            raise TimeoutError(f"Timed out waiting for {label}") from None
        if event[0] == "error":
            raise RuntimeError(event[1])
        result = predicate(event)
        if result is not None:
            return result


def send_command(client, command, timeout):
    sent = time.monotonic()
    result = client.publish(COMMAND_TOPIC, command, qos=1, retain=False)
    result.wait_for_publish(timeout=timeout)
    if not result.is_published():
        raise TimeoutError(f"Broker did not acknowledge {command}")
    return sent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--device", default="esp32-room1-dual-01")
    parser.add_argument("--timeout", type=float, default=30, help="Seconds allowed per step, including scheduled reporting gaps")
    parser.add_argument("--username")
    parser.add_argument("--password-env", default="MQTT_PASSWORD")
    parser.add_argument("--output", type=Path, help="Save actual observations to a new JSON file")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("Use a port from 1 to 65535 and a finite positive timeout")
    if args.output and args.output.exists():
        parser.error("Output already exists; choose a new filename")
    if mqtt is None:
        print("Install the dependencies first: python -m pip install -r requirements.txt", file=sys.stderr)
        return 1
    if args.username and args.password_env not in os.environ:
        parser.error("Set the MQTT password environment variable before using --username")

    events = queue.Queue()
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
                         client_id="esp32-check-" + uuid.uuid4().hex[:8],
                         protocol=mqtt.MQTTv311, reconnect_on_failure=False)
    client.connect_timeout = min(args.timeout, 10)
    if args.username:
        client.username_pw_set(args.username, os.environ[args.password_env])

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            events.put(("error", f"Broker rejected connection: {reason_code}"))
            return
        result, mid = client.subscribe([(TELEMETRY_TOPIC, 1), (STATE_TOPIC, 1)])
        if result != mqtt.MQTT_ERR_SUCCESS:
            events.put(("error", "Cannot request subscriptions"))

    def on_subscribe(client, userdata, mid, reason_codes, properties):
        if len(reason_codes) != 2 or any(code.is_failure for code in reason_codes):
            events.put(("error", "Broker rejected a required subscription"))
        else:
            events.put(("ready",))

    def on_message(client, userdata, message):
        events.put(("message", message.topic, bytes(message.payload), bool(message.retain), time.monotonic()))

    def on_disconnect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            events.put(("error", "Connection lost during the check"))

    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    changed_actuator = False
    passed = False
    records = []
    print("Keep dashboard AUTO OFF. Stop Python actuators and old boards before this check.")
    print("Watch the real TFT for rising PUB/RX counters and FAN ON then OFF.")
    try:
        client.connect(args.host, args.port, keepalive=30)
        client.loop_start()
        wait_event(events, lambda event: True if event[0] == "ready" else None, args.timeout, "SUBACK")

        def live_telemetry(event, after=0, previous_seq=0):
            if (event[0] != "message" or event[1] != TELEMETRY_TOPIC
                    or event[3] or event[4] < after):
                return None
            data = telemetry(event[2], args.device)
            return data if data and data["seq"] > previous_seq else None

        reading = wait_event(events, live_telemetry, args.timeout, "live telemetry from the ESP32")
        records.append({"step": "initial_telemetry", "payload": reading})
        print(f"PUB observed: device={args.device}, seq={reading['seq']}")
        for command in ("ON", "OFF"):
            changed_actuator = True
            sent = send_command(client, command, args.timeout)

            def applied_state(event):
                if (event[0] == "message" and event[1] == STATE_TOPIC and not event[3]
                        and event[4] >= sent and event[2] == command.encode("ascii")):
                    return command
                return None

            wait_event(events, applied_state, args.timeout, f"live {command} state feedback")
            records.append({"step": "command_feedback", "command": command})
            print(f"SUB feedback observed: FAN {command}")
            previous_seq = reading["seq"]
            reading = wait_event(events, lambda event: live_telemetry(event, sent, previous_seq),
                                 args.timeout, f"continuing telemetry after {command}")
            records.append({"step": f"telemetry_after_{command.lower()}", "payload": reading})
            print(f"PUB continues: seq={reading['seq']}")
        report = {
            "result": "PASS", "observed_at_utc": datetime.now(timezone.utc).isoformat(),
            "broker": {"host": args.host, "port": args.port}, "device": args.device,
            "observations": records,
            "scope": "Live MQTT observations; visually verify the real TFT separately.",
        }
        if args.output:
            with args.output.open("x", encoding="utf-8") as stream:
                json.dump(report, stream, indent=2)
                stream.write("\n")
        passed = True
        print("PASS: live ESP32 telemetry continued after ON and OFF feedback. Final state: OFF.")
        return 0
    except (OSError, ValueError, RuntimeError, TimeoutError, KeyboardInterrupt) as exc:
        print(f"CHECK FAILED: {exc}", file=sys.stderr)
        return 1
    finally:
        if changed_actuator and not passed and client.is_connected():
            try:
                send_command(client, "OFF", min(args.timeout, 3))
                print("Sent cleanup OFF; verify the actual TFT state.")
            except (OSError, ValueError, RuntimeError, TimeoutError):
                print("Cleanup OFF could not be acknowledged; verify the actual TFT state.", file=sys.stderr)
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    sys.exit(main())
