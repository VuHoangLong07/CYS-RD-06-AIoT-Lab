"""Deterministic model tests; no broker, Arduino libraries or hardware required."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from telemetry_model import Random, Room, Schedule
from temperature_publisher import environment_payload
from verify_device import telemetry


class TelemetryModelTests(unittest.TestCase):
    def test_seed_reproduces_random_stream(self):
        a, b, c = Random(20261018), Random(20261018), Random(17)
        first = [a.next() for _ in range(100)]
        self.assertEqual(first, [b.next() for _ in range(100)])
        self.assertNotEqual(first, [c.next() for _ in range(100)])

    def test_room_is_bounded_correlated_and_repeatable(self):
        a, b = Room(), Room()
        changes, previous = [], 0
        for now in range(0, 3600001, 500):
            changed = a.update(now)
            self.assertEqual(changed, b.update(now))
            self.assertEqual(a.fields(), b.fields())
            p = a.fields()
            for key, low, high in (("temperature",20,35),("humidity_pct",30,80),
                                   ("light_lux",0,1000),("pressure_hpa",990,1040),("co2_ppm",350,1500)):
                self.assertTrue(low <= p[key] <= high, (now, key, p[key]))
            if changed:
                self.assertTrue(30000 <= now-previous <= 90500)
                previous = now
                changes.append(now)
        self.assertGreater(len(changes), 30)

    def test_variable_delays_have_bounded_jitter_and_gaps(self):
        plan = Schedule()
        observed = set()
        reasons = set()
        for _ in range(1000):
            now = plan.environment_due
            plan.environment_attempted(now)
            delay = plan.environment_due-now
            self.assertTrue(4000 <= delay <= 6000 or 7500 <= delay <= 10000)
            observed.add(delay)
            reasons.add(plan.reason)
        self.assertGreater(len(observed), 50)
        self.assertIn("scheduled_gap", reasons)
        for now in range(100):
            plan.metrics_attempted(now)
            self.assertTrue(12000 <= plan.metrics_due-now <= 18000)

    def test_event_burst_has_only_two_followups(self):
        plan = Schedule()
        plan.occupancy_changed(10000)
        self.assertTrue(400 <= plan.environment_due-10000 <= 900)
        for _ in range(2):
            now = plan.environment_due
            plan.environment_attempted(now)
            self.assertTrue(800 <= plan.environment_due-now <= 1400)
        self.assertEqual(plan.burst_remaining, 0)
        now = plan.environment_due
        plan.environment_attempted(now)
        self.assertGreaterEqual(plan.environment_due-now, 4000)

    def test_fixed_profile_and_reconnect_do_not_catch_up(self):
        fixed = Schedule(variable=False)
        fixed.connected(0)
        fixed.occupancy_changed(1000)
        self.assertEqual(fixed.environment_due, 5000)
        fixed.environment_attempted(5000)
        self.assertEqual(fixed.environment_due, 10000)
        fixed.metrics_attempted(15000)
        self.assertEqual(fixed.metrics_due, 30000)
        variable = Schedule()
        variable.occupancy_changed(10)
        variable.connected(999999)
        self.assertEqual(variable.burst_remaining, 0)
        self.assertGreater(variable.environment_due, 999999)
        self.assertGreater(variable.metrics_due, 999999)

    def test_schema_labels_synthetic_fields_and_null_esp_measurements(self):
        payload = environment_payload(Room(), 1, 1000, 0, "variable-v2", 20261018, "startup", "fixture")
        self.assertEqual(payload["sensor"], "simulated")
        self.assertIsNone(payload["rssi_dbm"])
        self.assertIsNone(payload["free_heap_bytes"])
        self.assertIsNotNone(telemetry(json.dumps(payload).encode(), "sim-room1"))
        payload["occupied"] = "true"
        with self.assertRaises(ValueError):
            telemetry(json.dumps(payload).encode(), "sim-room1")


if __name__ == "__main__":
    unittest.main()
