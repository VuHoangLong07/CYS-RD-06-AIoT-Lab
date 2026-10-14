#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

os.chdir(Path(__file__).resolve().parent)

# Same arguments and defaults as capture_baseline.sh.
duration = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else "300"
output = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else "baseline.pcapng"

container = subprocess.run(
    ["sudo", "docker", "compose", "ps", "-q", "mosquitto"],
    stdout=subprocess.PIPE, text=True,
).stdout.strip()
broker_pid = subprocess.run(
    ["sudo", "docker", "inspect", "--format", "{{.State.Pid}}", container],
    stdout=subprocess.PIPE, text=True,
).stdout.strip()

capture_dir = Path(tempfile.mkdtemp(prefix="mqtt-capture.", dir="/tmp"))
os.chmod(capture_dir, 0o1777)
capture = capture_dir / "traffic.pcapng"

subprocess.run([
    "sudo", "nsenter", "-t", broker_pid, "-n", "--", "dumpcap",
    "-i", "eth0", "-f", "tcp port 1883", "-a", f"duration:{duration}",
    "-w", str(capture),
])
subprocess.run(["sudo", "chown", f"{os.getuid()}:{os.getgid()}", str(capture)])
shutil.move(str(capture), output)
capture_dir.rmdir()
