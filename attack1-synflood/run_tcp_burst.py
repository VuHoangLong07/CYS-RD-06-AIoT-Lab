#!/usr/bin/env python3
from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
import sys
import time

os.chdir(Path(__file__).resolve().parent)

def utc():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

container = subprocess.run(
    ["sudo", "docker", "compose", "ps", "-q", "mosquitto"],
    stdout=subprocess.PIPE, text=True,
).stdout.strip()
broker_ip = subprocess.run(
    ["sudo", "docker", "inspect", "--format",
     "{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}", container],
    stdout=subprocess.PIPE, text=True,
).stdout.strip()

run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")
run_dir = Path.cwd() / "results" / f"tcp-syn-{run_id}"
run_dir.mkdir(parents=True, exist_ok=True)
(run_dir / "metadata.txt").write_text(
    f"broker_ip={broker_ip}\n"
    "destination_port=1883\nsource_port=40000\nrate_pps=50\n"
    "probe_count=750\nbaseline_seconds=10\nrecovery_seconds=10\n"
    "capture_seconds=45\n"
)
timeline = run_dir / "timeline.csv"
timeline.write_text("event,time_utc\n")

def event(name):
    with timeline.open("a") as output:
        output.write(f"{name},{utc()}\n")

print(f"Results folder: {run_dir}", flush=True)
with (run_dir / "capture.log").open("w") as output:
    capture = subprocess.Popen(
        [sys.executable, "capture_baseline.py", "45", str(run_dir / "traffic.pcapng")],
        stdout=output, stderr=subprocess.STDOUT,
    )
time.sleep(2)

print("Baseline stage is starting", flush=True)
event("baseline_start")
time.sleep(10)
print("Baseline stage has ended", flush=True)

print("Attack stage is starting:", flush=True)
event("burst_start")
with (run_dir / "nping.log").open("w") as output:
    subprocess.run(
        ["sudo", "nping", "--tcp", "--flags", "syn", "--source-port", "40000",
         "-p", "1883", "--rate", "50", "--count", "750", broker_ip],
        stdout=output, stderr=subprocess.STDOUT,
    )
event("burst_end")
print("Attack stage has ended", flush=True)

print("Recovery stage is starting", flush=True)
time.sleep(10)
event("recovery_end")
capture.wait()
event("capture_wait_end")
print("Recovery stage has ended", flush=True)

fields = [
    "frame.number", "frame.time_epoch", "frame.len", "ip.src", "ip.dst",
    "tcp.stream", "tcp.srcport", "tcp.dstport", "tcp.flags.syn", "tcp.flags.ack",
    "tcp.flags.reset", "mqtt.msgtype", "mqtt.clientid", "mqtt.topic", "mqtt.qos",
    "mqtt.retain", "mqtt.msg",
]
command = [
    "tshark", "-r", str(run_dir / "traffic.pcapng"), "-Y", "tcp.port == 1883",
    "-T", "fields", "-E", "header=y", "-E", "separator=,", "-E", "quote=d",
    "-E", "occurrence=a",
]
for field in fields:
    command.extend(["-e", field])
with (run_dir / "traffic.csv").open("w") as output:
    subprocess.run(command, stdout=output)

syn_filter = (
    f"ip.dst == {broker_ip} && tcp.dstport == 1883 && tcp.srcport == 40000 "
    "&& tcp.flags.syn == 1 && tcp.flags.ack == 0"
)
syns = subprocess.run(
    ["tshark", "-r", str(run_dir / "traffic.pcapng"), "-Y", syn_filter,
     "-T", "fields", "-e", "frame.number"],
    stdout=subprocess.PIPE, text=True,
).stdout
(run_dir / "syn_count.txt").write_text(f"{syns.count(chr(10))}\n")
print("Simulation has ended", flush=True)
