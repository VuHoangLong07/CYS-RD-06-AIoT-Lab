#!/usr/bin/env bash
cd "$(dirname "$0")"

#find broker address
container=$(sudo docker compose ps -q mosquitto)
broker_ip=$(sudo docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$container")

#save metadata + other stuffs onto files
run_dir="$PWD/results/tcp-syn-$(date -u +%Y%m%dT%H%M%S)"
mkdir -p "$run_dir"
{
    echo "broker_ip=$broker_ip"
    echo "destination_port=1883"
    echo "source_port=40000"
    echo "rate_pps=50"
    echo "probe_count=750"
    echo "baseline_seconds=10"
    echo "recovery_seconds=10"
    echo "capture_seconds=45"
} > "$run_dir/metadata.txt"
echo "event,time_utc" > "$run_dir/timeline.csv"
echo "Results folder: $run_dir"

#start capture, everything including logs saved in a log file duh
bash capture_baseline.sh 45 "$run_dir/traffic.pcapng" > "$run_dir/capture.log" 2>&1 &
capture_pid=$!
sleep 2

#baseline stage
echo "Baseline stage is starting"
echo "baseline_start,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$run_dir/timeline.csv"
sleep 10
echo "Baseline stage has ended"

#tcp flood stage
echo "Attack stage is starting:"
echo "burst_start,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$run_dir/timeline.csv"
sudo nping --tcp --flags syn --source-port 40000 \
    -p 1883 --rate 50 --count 750 "$broker_ip" > "$run_dir/nping.log" 2>&1
echo "burst_end,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$run_dir/timeline.csv"
echo "Attack stage has ended"

#normal activity, after supposed eradication of the compromised node
echo "Recovery stage is starting"
sleep 10
echo "recovery_end,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$run_dir/timeline.csv"
wait "$capture_pid"
echo "capture_wait_end,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$run_dir/timeline.csv"
echo "Recovery stage has ended"

#export to csv for better ML readability
tshark -r "$run_dir/traffic.pcapng" -Y 'tcp.port == 1883' \
    -T fields -E header=y -E separator=, -E quote=d -E occurrence=a \
    -e frame.number -e frame.time_epoch -e frame.len \
    -e ip.src -e ip.dst -e tcp.stream -e tcp.srcport -e tcp.dstport \
    -e tcp.flags.syn -e tcp.flags.ack -e tcp.flags.reset \
    -e mqtt.msgtype -e mqtt.clientid -e mqtt.topic -e mqtt.qos \
    -e mqtt.retain -e mqtt.msg > "$run_dir/traffic.csv"

#counts num for syn, somewhat helpful
tshark -r "$run_dir/traffic.pcapng" \
    -Y "ip.dst == $broker_ip && tcp.dstport == 1883 && tcp.srcport == 40000 && tcp.flags.syn == 1 && tcp.flags.ack == 0" \
    -T fields -e frame.number | wc -l > "$run_dir/syn_count.txt"
echo "Simulation has ended"
