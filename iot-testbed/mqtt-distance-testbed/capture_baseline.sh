#!/usr/bin/env bash
# Run from this Compose project, while Mosquitto is running.
# Captures real traffic at the broker's eth0 using Wireshark's dumpcap engine.
set -euo pipefail

duration="${1:-300}"
output="${2:-baseline.pcapng}"
if [[ ! "$duration" =~ ^[1-9][0-9]*$ ]]; then
  echo "Usage: bash capture_baseline.sh [seconds] [output.pcapng]" >&2
  exit 1
fi
for program in docker nsenter dumpcap; do
  if ! command -v "$program" >/dev/null 2>&1; then
    echo "Missing $program. See CAPTURE_BASELINE.md for setup." >&2
    exit 1
  fi
done
if [[ -e "$output" ]]; then
  echo "File already exists: $output. Choose a new output filename." >&2
  exit 1
fi
output="$(realpath -m -- "$output")"
capture_uid="$(id -u)"
capture_gid="$(id -g)"
restore_owner() {
  if [[ -f "$output" ]]; then
    sudo chown "$capture_uid:$capture_gid" -- "$output" || true
  fi
}
trap restore_owner EXIT
broker_id="$(sudo docker compose ps -q mosquitto)"
if [[ -z "$broker_id" ]]; then
  echo "Mosquitto is not running. Run sudo docker compose up -d first." >&2
  exit 1
fi
broker_pid="$(sudo docker inspect --format '{{.State.Pid}}' "$broker_id")"
if [[ ! "$broker_pid" =~ ^[1-9][0-9]*$ ]]; then
  echo "Mosquitto has no running process." >&2
  exit 1
fi

echo "Capturing broker eth0 for $duration seconds to $output"
echo "After capture starts, start the publisher and actuator for this stage."
echo "Use the dashboard normally: ON/OFF controls, then automatic mode."
# Only the network namespace changes; the host dumpcap and output path remain available.
# Broker-side port 1883 stays the same even if the laptop publishes port 1884.
sudo nsenter --target "$broker_pid" --net -- \
  dumpcap -i eth0 -f 'tcp port 1883' -a "duration:$duration" -w "$output"
echo "Saved $output. Open it with: wireshark $output"
