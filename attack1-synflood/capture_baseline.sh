#!/usr/bin/env bash
cd "$(dirname "$0")"

#args
duration=${1:-300}
output=${2:-baseline.pcapng}

#finds the container and process id of the broker
container=$(sudo docker compose ps -q mosquitto)
broker_pid=$(sudo docker inspect --format '{{.State.Pid}}' "$container")

#old vers restricted access to the capture file. Simple chmod was issued
capture_dir=$(mktemp -d /tmp/mqtt-capture.XXXXXX)
chmod 1777 "$capture_dir"

#records traffic to tcp port 1883 and writes to the file as traffic.pcapng
sudo nsenter -t "$broker_pid" -n -- dumpcap \
    -i eth0 -f 'tcp port 1883' -a "duration:$duration" \
    -w "$capture_dir/traffic.pcapng"

# Give the capture to your user and move it into the requested location.
sudo chown "$(id -u):$(id -g)" "$capture_dir/traffic.pcapng"
mv "$capture_dir/traffic.pcapng" "$output"
rmdir "$capture_dir"
