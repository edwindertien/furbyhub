#pragma once
// Presence decision for a distance sensor - pure logic, unit-tested on the host
// (test/sensor_check.cpp).
//
//   * a median of the last 3 readings removes single spikes (typical for ultrasonic)
//   * hysteresis: "present" below wakeMm, "absent" above sleepMm, in between nothing changes
//   * a change only counts once it has held for smoothMs
//   * a reading of -1 (no echo / out of range) means nobody close: counts as far
#include <stdint.h>

namespace SensorLogic {

inline int32_t median3(int32_t a, int32_t b, int32_t c) {
  // -1 (no echo) sorts as "very far"
  auto key = [](int32_t x) { return x < 0 ? 100000 : x; };
  int32_t ka = key(a), kb = key(b), kc = key(c);
  int32_t m = ka > kb ? (kb > kc ? kb : (ka > kc ? kc : ka)) : (ka > kc ? ka : (kb > kc ? kc : kb));
  return m == 100000 ? -1 : m;
}

struct Presence {
  bool     present = false;
  bool     candidate = false;
  uint32_t candidateSince = 0;

  bool update(int32_t mm, uint32_t nowMs, uint16_t wakeMm, uint16_t sleepMm, uint16_t smoothMs) {
    const bool nearEnough = (mm >= 0 && mm < wakeMm);
    const bool farEnough  = (mm < 0 || mm > sleepMm);
    bool want = present;
    if (nearEnough) want = true;
    else if (farEnough) want = false;

    if (want == present) { candidate = present; return present; }
    if (candidate != want) { candidate = want; candidateSince = nowMs; }
    if ((uint32_t)(nowMs - candidateSince) >= smoothMs) present = want;
    return present;
  }
};

}  // namespace SensorLogic
