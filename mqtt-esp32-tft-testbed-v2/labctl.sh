#!/usr/bin/env bash
# Linux laptop helper. All MQTT CLI operations run inside the Mosquitto container.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

usage() {
  cat <<'USAGE'
Usage: bash labctl.sh up|status|logs|watch|on|off
  up      Validate and start the broker and Node-RED with the physical LAN binding.
  status  Show running Compose services.
  logs    Show recent broker and Node-RED logs.
  watch   Subscribe to lab/# at QoS 1; Ctrl+C stops watching.
  on/off  Send an exact, nonretained command. Keep dashboard AUTO off for manual tests.
Run python3 configure_lab.py first. This helper uses the supplied anonymous broker.
For a custom authenticated broker, use its existing configuration and client credentials.
USAGE
}

if [[ $# -ne 1 ]]; then usage >&2; exit 2; fi
case "$1" in
  -h|--help|help) usage; exit 0 ;;
  up|status|logs|watch|on|off) ;;
  *) usage >&2; exit 2 ;;
esac
if [[ ! -f .env ]]; then
  echo "Run python3 configure_lab.py to set the laptop LAN address first." >&2
  exit 1
fi
if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is missing. Install Docker Engine/Desktop with the Compose plugin first." >&2
  exit 1
fi
if docker info >/dev/null 2>&1; then
  docker_cmd=(docker)
elif command -v sudo >/dev/null 2>&1; then
  docker_cmd=(sudo docker)
else
  echo "Cannot access the Docker daemon; start it and check your Docker permissions." >&2
  exit 1
fi
compose=("${docker_cmd[@]}" compose -f compose.yaml -f compose.physical.yaml)
case "$1" in
  up)
    "${compose[@]}" config --quiet
    "${compose[@]}" up -d
    "${compose[@]}" ps
    echo "Open http://127.0.0.1:1880 and import node-red-esp32-flow.json."
    echo "Use the NODE_RED_PORT from .env if it differs from 1880."
    ;;
  status) "${compose[@]}" ps ;;
  logs) "${compose[@]}" logs --tail=50 mosquitto node-red ;;
  watch)
    "${compose[@]}" exec -T mosquitto mosquitto_sub -h 127.0.0.1 -p 1883 -q 1 -t 'lab/#' -v
    ;;
  on|off)
    "${compose[@]}" exec -T mosquitto mosquitto_pub -h 127.0.0.1 -p 1883 \
      -q 1 -t 'lab/room1/fan/set' -m "${1^^}"
    echo "Command sent. Check fan/state and the TFT for device feedback."
    ;;
esac
