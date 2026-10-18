#pragma once
#include <stdint.h>
#include <math.h>

// Synthetic room dynamics and bounded application scheduling, independent of Wi-Fi.
// Unsigned elapsed-time comparisons continue to work across millis() rollover.
namespace RoomLab {
struct Random {
  uint32_t state;
  explicit Random(uint32_t seed) : state(seed ? seed : 1) {}
  uint32_t next() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
  uint32_t between(uint32_t low, uint32_t high) { return low + next() % (high - low + 1); }
  float noise(float amplitude) { return ((int32_t)between(0, 2000) - 1000) * amplitude / 1000.0f; }
};

struct Timer {
  uint32_t start = 0, wait = 0;
  void after(uint32_t now, uint32_t delay) { start = now; wait = delay; }
  bool due(uint32_t now) const { return (uint32_t)(now - start) >= wait; }
  uint32_t remaining(uint32_t now) const {
    uint32_t elapsed = now - start;
    return elapsed >= wait ? 0 : wait - elapsed;
  }
};

struct Room {
  Random random;
  Timer occupancyTimer;
  uint32_t lastUpdate = 0, revision = 0;
  uint64_t elapsedMs = 0;
  float temperature = 26.5f, humidity = 55.0f, light = 20.0f;
  float pressure = 1013.0f, co2 = 450.0f;
  bool occupied = false;
  explicit Room(uint32_t seed) : random(seed ^ 0xA341316Cu) {}
  void begin(uint32_t now) {
    lastUpdate = now;
    occupancyTimer.after(now, random.between(30000, 90000));
  }
  bool update(uint32_t now, bool fan) {
    uint32_t delta = now - lastUpdate;
    if (delta < 500) return false;
    lastUpdate = now;
    elapsedMs += delta;
    // Long loop/network delays advance phase without a burst of catch-up updates.
    float dt = delta / 1000.0f;
    if (dt > 10.0f) dt = 10.0f;
    float t = (float)(elapsedMs / 1000.0);
    bool changed = occupancyTimer.due(now);
    if (changed) {
      occupied = !occupied;
      ++revision;
      occupancyTimer.after(now, random.between(30000, 90000));
    }
    float thermalTarget = 27.2f + 2.0f * sinf(t * 6.2831853f / 240.0f)
                        + (occupied ? 0.25f : 0.0f) - (fan ? 0.4f : 0.0f);
    float thermalAlpha = dt / 30.0f;
    temperature += thermalAlpha * (thermalTarget - temperature) + random.noise(0.008f);
    humidity += (dt / 45.0f) * (55.0f - 0.8f * (temperature - 26.5f) - humidity)
              + random.noise(0.015f);
    pressure += (dt / 90.0f) * (1013.0f + 2.0f * sinf(t / 180.0f) - pressure)
              + random.noise(0.002f);
    float lightTarget = occupied ? 350.0f + 70.0f * sinf(t / 80.0f) : 20.0f;
    float lightAlpha = dt / 4.0f;
    if (lightAlpha > 1.0f) lightAlpha = 1.0f;
    light += lightAlpha * (lightTarget - light) + random.noise(0.2f);
    float co2Target = (occupied ? 1100.0f : 450.0f) - (fan ? 50.0f : 0.0f);
    co2 += (dt / 75.0f) * (co2Target - co2) + random.noise(0.1f);
    return changed;
  }
};

struct Schedule {
  Random random;
  Timer environment, metrics;
  bool variable;
  uint32_t baseEnvironment, baseMetrics;
  uint8_t burstRemaining = 0;
  const char* reason = "startup";
  Schedule(uint32_t seed, bool irregular, uint32_t environmentMs, uint32_t metricsMs)
    : random(seed ^ 0xC8013EA4u), variable(irregular),
      baseEnvironment(environmentMs), baseMetrics(metricsMs) {}
  uint32_t jitter(uint32_t base) {
    return variable ? random.between(base * 8 / 10, base * 12 / 10) : base;
  }
  void connected(uint32_t now) {
    burstRemaining = 0;
    reason = "startup";
    environment.after(now, variable ? random.between(800, 1800) : baseEnvironment);
    metrics.after(now, variable ? random.between(2000, 5000) : baseMetrics);
  }
  void occupancyChanged(uint32_t now) {
    if (!variable) return;
    // One prompt reading plus two follow-ups, at most one burst per occupancy change.
    burstRemaining = 2;
    reason = "occupancy_change";
    environment.after(now, random.between(400, 900));
  }
  void environmentAttempted(uint32_t now) {
    if (variable && burstRemaining) {
      --burstRemaining;
      reason = "event_burst";
      environment.after(now, random.between(800, 1400));
    } else if (variable && random.between(1, 100) <= 4) {
      reason = "scheduled_gap";
      environment.after(now, random.between(baseEnvironment * 15 / 10, baseEnvironment * 2));
    } else {
      reason = "periodic";
      environment.after(now, jitter(baseEnvironment));
    }
  }
  void metricsAttempted(uint32_t now) { metrics.after(now, jitter(baseMetrics)); }
};
} // namespace RoomLab
