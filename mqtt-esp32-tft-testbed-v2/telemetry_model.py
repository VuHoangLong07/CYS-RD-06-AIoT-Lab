"""Synthetic room dynamics and bounded reporting schedule matching the firmware."""
import math

MODEL_SEED = 20261018
SIMULATED_FIELDS = ["temperature", "humidity_pct", "light_lux", "pressure_hpa", "co2_ppm", "occupied"]


class Random:
    def __init__(self, seed):
        self.state = (seed & 0xffffffff) or 1

    def next(self):
        self.state ^= (self.state << 13) & 0xffffffff
        self.state ^= self.state >> 17
        self.state ^= (self.state << 5) & 0xffffffff
        self.state &= 0xffffffff
        return self.state

    def between(self, low, high):
        return low + self.next() % (high - low + 1)

    def noise(self, amplitude):
        return (self.between(0, 2000) - 1000) * amplitude / 1000


class Room:
    def __init__(self, seed=MODEL_SEED):
        self.random = Random(seed ^ 0xA341316C)
        self.last_update = 0
        self.elapsed_ms = 0
        self.occupancy_due = self.random.between(30000, 90000)
        self.revision = 0
        self.temperature = 26.5
        self.humidity = 55.0
        self.light = 20.0
        self.pressure = 1013.0
        self.co2 = 450.0
        self.occupied = False

    def update(self, now, fan=False):
        delta = now - self.last_update
        if delta < 500:
            return False
        self.last_update = now
        self.elapsed_ms += delta
        dt = min(delta / 1000, 10)
        t = self.elapsed_ms / 1000
        changed = now >= self.occupancy_due
        if changed:
            self.occupied = not self.occupied
            self.revision += 1
            self.occupancy_due = now + self.random.between(30000, 90000)
        thermal_target = 27.2 + 2 * math.sin(t * 6.2831853 / 240) + (0.25 if self.occupied else 0) - (0.4 if fan else 0)
        self.temperature += (dt / 30) * (thermal_target - self.temperature) + self.random.noise(0.008)
        self.humidity += (dt / 45) * (55 - 0.8 * (self.temperature - 26.5) - self.humidity) + self.random.noise(0.015)
        self.pressure += (dt / 90) * (1013 + 2 * math.sin(t / 180) - self.pressure) + self.random.noise(0.002)
        light_target = 350 + 70 * math.sin(t / 80) if self.occupied else 20
        self.light += min(dt / 4, 1) * (light_target - self.light) + self.random.noise(0.2)
        co2_target = (1100 if self.occupied else 450) - (50 if fan else 0)
        self.co2 += (dt / 75) * (co2_target - self.co2) + self.random.noise(0.1)
        return changed

    def fields(self):
        return dict(temperature=round(self.temperature, 2), humidity_pct=round(self.humidity, 1),
                    light_lux=round(self.light, 1), pressure_hpa=round(self.pressure, 2),
                    co2_ppm=round(self.co2, 1), occupied=self.occupied)


class Schedule:
    def __init__(self, seed=MODEL_SEED, variable=True, environment_ms=5000, metrics_ms=15000):
        self.random = Random(seed ^ 0xC8013EA4)
        self.variable = variable
        self.environment_ms, self.metrics_ms = environment_ms, metrics_ms
        self.burst_remaining = 0
        self.reason = "startup"
        self.environment_due = self.metrics_due = 0

    def jitter(self, base):
        return self.random.between(base * 8 // 10, base * 12 // 10) if self.variable else base

    def connected(self, now):
        self.burst_remaining = 0
        self.reason = "startup"
        self.environment_due = now + (self.random.between(800, 1800) if self.variable else self.environment_ms)
        self.metrics_due = now + (self.random.between(2000, 5000) if self.variable else self.metrics_ms)

    def occupancy_changed(self, now):
        if self.variable:
            self.burst_remaining = 2
            self.reason = "occupancy_change"
            self.environment_due = now + self.random.between(400, 900)

    def environment_attempted(self, now):
        if self.variable and self.burst_remaining:
            self.burst_remaining -= 1
            self.reason = "event_burst"
            delay = self.random.between(800, 1400)
        elif self.variable and self.random.between(1, 100) <= 4:
            self.reason = "scheduled_gap"
            delay = self.random.between(self.environment_ms * 15 // 10, self.environment_ms * 2)
        else:
            self.reason = "periodic"
            delay = self.jitter(self.environment_ms)
        self.environment_due = now + delay

    def metrics_attempted(self, now):
        self.metrics_due = now + self.jitter(self.metrics_ms)
