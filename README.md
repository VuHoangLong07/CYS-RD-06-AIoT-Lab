# CYS-RD-06  Behavioral Detection and Security Monitoring for IoT Devices

> Lightweight behavioral and network monitoring for detecting abnormal or potentially malicious activity in resource-constrained IoT environments.

## Overview

This project investigates whether lightweight network and behavioral monitoring techniques can reliably detect abnormal or potentially malicious activity in resource-constrained IoT devices.

The project focuses on establishing **normal behavior baselines** for IoT devices and identifying deviations from these baselines using multiple detection approaches. The study will evaluate rule-based, protocol-level, and statistical/ML-based techniques across different IoT device profiles and simulated attack scenarios.

The project will use a controlled IoT security testbed consisting of devices such as Raspberry Pi and ESP32, together with network monitoring and security tools.

## Research Question

> **Can lightweight network and behavioral monitoring reliably detect abnormal or potentially malicious activity related to resource-constrained IoT devices?**

## Research Objectives

The project aims to:
- Characterize normal IoT device behavior, including:
  - Connection destinations
  - Network protocols
  - Connection frequency
  - Packet volume
  - MQTT/DNS activity
  - Protocol and traffic patterns

- Establish normal-behavior baselines for different IoT device profiles.

- Introduce controlled abnormal and adversarial behaviors within an authorized testbed.

- Develop and evaluate multiple detection approaches, including:
  - Rule-based detection
  - Network intrusion detection
  - Protocol-level analysis
  - Statistical anomaly detection
  - Machine-learning-based detection where appropriate

- Measure detection performance using:
  - Detection rate
  - False-positive rate
  - False-negative rate
  - Detection latency
  - Computational/resource overhead

- Compare detection effectiveness and resource requirements across different IoT device types.
