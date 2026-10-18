// Compile on a desktop: g++ -std=c++17 tests/test_firmware_model.cpp -o /tmp/room-model-check
#include "../firmware/esp32_pubsub_tft/telemetry_model.h"
#include <assert.h>
#include <iostream>
int main() {
  RoomLab::Timer timer;
  timer.after(0xffffff00u, 500);
  assert(!timer.due(243) && timer.remaining(243)==1 && timer.due(244));
  RoomLab::Schedule fixed(20261018, false, 5000, 15000);
  fixed.connected(0);
  fixed.occupancyChanged(1000);
  assert(fixed.environment.wait==5000);
  fixed.environmentAttempted(5000);
  assert(fixed.environment.start==5000 && fixed.environment.wait==5000);
  RoomLab::Room room(20261018);
  RoomLab::Schedule schedule(20261018, true, 5000, 15000);
  room.begin(0);
  schedule.connected(0);
  unsigned environmentCount=0, eventCount=0;
  for(uint32_t now=0; now<=600000; now+=100) {
    if(room.update(now, false)) { schedule.occupancyChanged(now); ++eventCount; }
    if(schedule.environment.due(now)) {
      assert(room.temperature>=20 && room.temperature<=35);
      assert(room.co2>=350 && room.co2<=1500);
      std::cout << now << ',' << room.temperature << ',' << room.humidity << ','
                << room.light << ',' << room.pressure << ',' << room.co2 << ',' << room.occupied << '\n';
      schedule.environmentAttempted(now);
      ++environmentCount;
    }
  }
  assert(environmentCount>100 && eventCount>=5);
}
