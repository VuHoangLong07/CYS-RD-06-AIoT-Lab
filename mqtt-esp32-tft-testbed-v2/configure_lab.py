#!/usr/bin/env python3
"""Set the same laptop broker address in Compose and ESP32 firmware."""

import argparse
import getpass
import ipaddress
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
HEADER = ROOT / "firmware/esp32_pubsub_tft/lab_config.h"


def current_wifi_ip():
    try:
        result = subprocess.run(
            ["ip", "-j", "-4", "addr", "show", "dev", "wlo1"],
            check=True, capture_output=True, text=True, timeout=3,
        )
        return next(
            address["local"] for interface in json.loads(result.stdout)
            for address in interface.get("addr_info", [])
            if address.get("scope") == "global"
        )
    except (OSError, subprocess.SubprocessError, ValueError, StopIteration, KeyError):
        return ""


def prompt(label, default=""):
    value = input(label + (f" [{default}]" if default else "") + ": ").strip()
    return value or default


def set_env_value(text, name, value):
    line = f"{name}={value}"
    pattern = rf"(?m)^{re.escape(name)}=.*$"
    if len(re.findall(pattern, text)) > 1:
        raise ValueError(f"Remove duplicate {name} entries in .env first")
    if re.search(pattern, text):
        return re.sub(pattern, lambda match: line, text)
    return text.rstrip() + "\n" + line + "\n"


def set_cpp_string(text, name, value):
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ValueError(f"{name} must not contain control characters")
    pattern = rf'(?m)^(const char {re.escape(name)}\[\] = )"(?:[^"\\]|\\.)*"(;)'
    replacement = json.dumps(value, ensure_ascii=False)
    changed, count = re.subn(pattern, lambda match: match[1] + replacement + match[2], text)
    if count != 1:
        raise ValueError(f"Expected one {name} definition in lab_config.h")
    return changed


def env_port(text, name, default):
    values = re.findall(rf"(?m)^{re.escape(name)}=(.*)$", text)
    if len(values) > 1:
        raise ValueError(f"Remove duplicate {name} entries in .env first")
    raw = values[0].split("#", 1)[0].strip().strip("\"'") if values else str(default)
    if not raw.isdecimal() or not 1 <= int(raw) <= 65535:
        raise ValueError(f"{name} must be between 1 and 65535")
    return int(raw)


def write_private(path, text):
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(text)
            stream.flush()
            os.fchmod(stream.fileno(), 0o600)
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lan-ip", help="Current IPv4 address of your laptop on the ESP32's LAN")
    parser.add_argument("--ssid", help="2.4 GHz Wi-Fi network name; omit to enter it interactively")
    parser.add_argument("--wifi-password-env", help="Read the Wi-Fi password from this environment variable")
    args = parser.parse_args()
    try:
        header = HEADER.read_text(encoding="utf-8")
        env_path = ROOT / ".env"
        env_text = (env_path if env_path.exists() else ROOT / ".env.example").read_text()
        lan_ip = args.lan_ip or prompt("Current laptop LAN IPv4", current_wifi_ip())
        address = ipaddress.IPv4Address(lan_ip)
        if address.is_loopback or address.is_unspecified or address.is_multicast or int(address) == 0xFFFFFFFF:
            raise ValueError("Use the laptop's reachable LAN IPv4, not localhost or a wildcard")
        ssid = args.ssid if args.ssid is not None else input("2.4 GHz Wi-Fi SSID: ")
        if not 1 <= len(ssid.encode("utf-8")) <= 32:
            raise ValueError("Wi-Fi SSID must contain 1 to 32 UTF-8 bytes")
        if args.wifi_password_env:
            if args.wifi_password_env not in os.environ:
                raise ValueError("The requested Wi-Fi password environment variable is unset")
            password = os.environ[args.wifi_password_env]
        else:
            password = getpass.getpass("Wi-Fi password (hidden; empty for an open lab network): ")
        ports = {}
        for name, default in (("MQTT_PORT", 1883), ("NODE_RED_PORT", 1880)):
            ports[name] = env_port(env_text, name, default)
        if ports["MQTT_PORT"] == ports["NODE_RED_PORT"]:
            raise ValueError("MQTT_PORT and NODE_RED_PORT must be different")
        for name, value in (("WIFI_SSID", ssid), ("WIFI_PASSWORD", password), ("MQTT_HOST", str(address))):
            header = set_cpp_string(header, name, value)
        header, count = re.subn(
            r"(?m)^(const unsigned short MQTT_PORT = )[0-9]+(;)" ,
            lambda match: match[1] + str(ports["MQTT_PORT"]) + match[2], header,
        )
        if count != 1:
            raise ValueError("Expected one MQTT_PORT definition in lab_config.h")
        env_text = set_env_value(env_text, "LAB_LAN_IP", str(address))
        for name, value in ports.items():
            env_text = set_env_value(env_text, name, str(value))
        # Validate all inputs and source definitions before changing either file.
        write_private(HEADER, header)
        write_private(env_path, env_text)
        print(f"Configured broker {address}:{ports['MQTT_PORT']} in .env and lab_config.h.")
        print("Wi-Fi credentials were saved locally without being printed.")
        print("Next: bash labctl.sh up, then upload the TFT test and combined sketch.")
        return 0
    except (OSError, ValueError) as exc:
        print(f"Configuration failed: {exc}", file=sys.stderr)
        return 1
    except (EOFError, KeyboardInterrupt):
        print("Configuration cancelled.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
