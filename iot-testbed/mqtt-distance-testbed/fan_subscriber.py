#!/usr/bin/env python3
"""Act as a virtual fan: receive ON/OFF commands and report the current state."""

import argparse
import sys
import uuid
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

COMMAND_TOPIC = "lab/room1/fan/set"
STATE_TOPIC = "lab/room1/fan/state"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    args = parser.parse_args()
    fan_state = "OFF"
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="virtual-fan-" + uuid.uuid4().hex[:8],
        protocol=mqtt.MQTTv311,
    )

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"Connection rejected: {reason_code}", flush=True)
            return
        # Re-subscribe after every reconnect. Publish the fan's current state.
        client.subscribe(COMMAND_TOPIC, qos=1)
        client.publish(STATE_TOPIC, fan_state, qos=1, retain=True)
        print(f"Connected to {args.host}:{args.port}; virtual fan is {fan_state}.", flush=True)

    def on_subscribe(client, userdata, mid, reason_codes, properties):
        if any(code.is_failure for code in reason_codes):
            print("Subscription rejected by the broker.", flush=True)
        else:
            print(f"Ready: subscribed to {COMMAND_TOPIC}; waiting for ON/OFF.", flush=True)

    def on_message(client, userdata, message):
        nonlocal fan_state
        try:
            command = message.payload.decode("utf-8")
        except UnicodeDecodeError:
            print("IGNORED: fan command is not UTF-8 text.", flush=True)
            return
        if message.retain:
            print("IGNORED: retained command; send a new nonretained ON/OFF.", flush=True)
            return
        if command not in {"ON", "OFF"}:
            print(f"IGNORED: expected ON or OFF, received {command!r}.", flush=True)
            return
        changed = command != fan_state
        fan_state = command
        stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
        print(f"{stamp} RECEIVED {COMMAND_TOPIC} {command} -> FAN {fan_state}"
              + (" (changed)" if changed else " (unchanged)"), flush=True)
        # This confirms the virtual fan processed the command.
        client.publish(STATE_TOPIC, fan_state, qos=1, retain=True)

    def on_disconnect(client, userdata, flags, reason_code, properties):
        nonlocal fan_state
        fan_state = "OFF"
        if reason_code.is_failure:
            print("Broker disconnected; the MQTT client will try to reconnect.", flush=True)

    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    try:
        client.connect(args.host, args.port, keepalive=60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nVirtual fan stopped.", flush=True)
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"MQTT error: {exc}. Check docker compose ps and your broker port.", file=sys.stderr)
        return 1
    finally:
        client.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
