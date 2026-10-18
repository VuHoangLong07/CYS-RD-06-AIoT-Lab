#!/usr/bin/env python3
"""Software counterpart of rich ESP32 room telemetry and irregular timing."""
import argparse
from datetime import datetime, timezone
import json
import math
import os
import sys
import threading
import time
import uuid
from telemetry_model import MODEL_SEED, SIMULATED_FIELDS, Room, Schedule
try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

TOPIC = "lab/esp32/telemetry"
METRICS_TOPIC = "lab/esp32/metrics"
EVENTS_TOPIC = "lab/esp32/events"
FIRMWARE = "python-room-2.0.0"


def environment_payload(room, seq, now, gap, profile, seed, reason, boot_id):
    return dict(schema=2, device="sim-room1", firmware=FIRMWARE, boot_id=boot_id,
                seq=seq, uptime_ms=now, timestamp=datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                **room.fields(), unit="C", sensor="simulated", simulated_fields=SIMULATED_FIELDS,
                fan_state=None, rssi_dbm=None, free_heap_bytes=None, publish_gap_ms=gap,
                timing_profile=profile, model_seed=seed, reason=reason)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--interval", type=float, default=5, help="Environment base seconds, 0.5..60; variable mode adds jitter")
    parser.add_argument("--profile", choices=("variable", "fixed"), default="variable")
    parser.add_argument("--seed", type=int, default=MODEL_SEED)
    parser.add_argument("--value", type=float, help="Override the simulated temperature for controlled tests")
    parser.add_argument("--count", type=int, default=0, help="Stop after N environmental readings; 0 means forever")
    parser.add_argument("--username")
    parser.add_argument("--password-env", default="MQTT_PASSWORD")
    args = parser.parse_args()
    if not math.isfinite(args.interval) or not 0.5 <= args.interval <= 60 or args.count < 0:
        parser.error("Use --interval 0.5..60 seconds and a nonnegative --count")
    if not 1 <= args.port <= 65535 or not 0 <= args.seed <= 0xffffffff:
        parser.error("Use a port from 1 to 65535 and a seed from 0 to 4294967295")
    if args.value is not None and not math.isfinite(args.value):
        parser.error("--value must be finite")
    if mqtt is None:
        print("Install requirements.txt in your virtual environment first.", file=sys.stderr)
        return 1
    connected = threading.Event()
    generation = 0
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
                         client_id="room-simulator-" + uuid.uuid4().hex[:8], protocol=mqtt.MQTTv311)
    if args.username:
        client.username_pw_set(args.username, os.environ.get(args.password_env))

    def on_connect(client, userdata, flags, reason_code, properties):
        nonlocal generation
        if reason_code.is_failure:
            print(f"Connection rejected: {reason_code}", flush=True)
            return
        generation += 1
        connected.set()
        print(f"Connected: {args.host}:{args.port}; profile={args.profile}-v2 seed={args.seed}", flush=True)

    def on_disconnect(client, userdata, flags, reason_code, properties):
        connected.clear()
        if reason_code.is_failure:
            print("Broker disconnected; waiting for reconnection.", flush=True)

    client.on_connect, client.on_disconnect = on_connect, on_disconnect
    room = Room(args.seed)
    schedule = Schedule(args.seed, args.profile == "variable", round(args.interval * 1000))
    profile = args.profile + "-v2"
    boot_id = uuid.uuid4().hex[:8]
    seq = metrics_seq = event_seq = 0
    env_acks = metrics_acks = event_acks = 0
    seen_generation = -1
    pending_event = False
    last_environment = 0
    cursor = 0
    started = time.monotonic()

    def publish(topic, payload):
        result = client.publish(topic, json.dumps(payload, separators=(",", ":"), allow_nan=False), qos=1, retain=False)
        result.wait_for_publish(timeout=10)
        if not result.is_published():
            raise TimeoutError("Broker did not acknowledge publication")
        print("ACK " + topic + " " + json.dumps(payload, separators=(",", ":")), flush=True)

    try:
        client.connect(args.host, args.port, keepalive=60)
        client.loop_start()
        while args.count == 0 or seq < args.count:
            if not connected.wait(10):
                raise TimeoutError("No successful connection/reconnection within 10 seconds")
            now = int((time.monotonic() - started) * 1000)
            if room.update(now):
                pending_event = True
                schedule.occupancy_changed(now)
            if args.value is not None:
                room.temperature = args.value
            if generation != seen_generation:
                schedule.connected(now)
                pending_event = False
                seen_generation = generation
            for offset in range(3):
                stream = (cursor + offset) % 3
                if stream == 0 and now >= schedule.environment_due:
                    gap = now - last_environment if seq else 0
                    last_environment = now
                    seq += 1
                    payload = environment_payload(room, seq, now, gap, profile, args.seed, schedule.reason, boot_id)
                    if args.value is not None:
                        payload["temperature_override"] = True
                    publish(TOPIC, payload)
                    env_acks += 1
                    schedule.environment_attempted(int((time.monotonic() - started) * 1000))
                elif stream == 1 and now >= schedule.metrics_due:
                    metrics_seq += 1
                    # ESP32-only measurements are null rather than fabricated on the laptop.
                    publish(METRICS_TOPIC, dict(schema=2, device="sim-room1", firmware=FIRMWARE,
                            boot_id=boot_id, seq=metrics_seq, uptime_ms=now, source="python_simulator",
                            free_heap_bytes=None, min_free_heap_bytes=None, rssi_dbm=None, wifi_channel=None,
                            env_pub_ack_count=env_acks, metrics_pub_ack_count=metrics_acks,
                            event_pub_ack_count=event_acks, timing_profile=profile, model_seed=args.seed))
                    metrics_acks += 1
                    schedule.metrics_attempted(int((time.monotonic() - started) * 1000))
                elif stream == 2 and pending_event:
                    event_seq += 1
                    publish(EVENTS_TOPIC, dict(schema=2, device="sim-room1", firmware=FIRMWARE,
                            boot_id=boot_id, seq=event_seq, uptime_ms=now, event="occupancy_changed",
                            source="simulated", occupied=room.occupied, revision=room.revision))
                    event_acks += 1
                    pending_event = False
                else:
                    continue
                cursor = (stream + 1) % 3
                break
            time.sleep(0.02)
    except KeyboardInterrupt:
        print("\nRoom publisher stopped.", flush=True)
    except (OSError, RuntimeError, TimeoutError, ValueError) as exc:
        print(f"MQTT error: {exc}", file=sys.stderr)
        return 1
    finally:
        client.disconnect()
        client.loop_stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
