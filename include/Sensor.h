#pragma once
#include <Arduino.h>

// Presence / distance sensor of this hub. Type and thresholds come from furbyhub.json:
//
//   sensor.type    none | pir | hcsr04
//   sensor.wake_mm   closer than this = someone is there        (distance sensors)
//   sensor.sleep_mm  farther than this = gone (>= wake_mm; in between nothing changes)
//   sensor.smooth_ms a change must hold this long before it counts
//
// pir     digital output on PIN_SENSOR (GP22), active high
// hcsr04  ultrasonic: trigger on PIN_TRIG (GP20), echo on PIN_ECHO (GP19).
//         The classic HC-SR04 drives its echo pin at 5 V and the Pico is NOT 5 V tolerant:
//         use a 3.3 V module (HC-SR04P, RCWL-1601 ...) or a resistor divider on the echo line.
//
// Changing the type needs a reboot (config set sensor.type ..., save, reboot).
class SensorClass {
public:
  void begin();
  void loop();

  bool        present() const { return _present; }
  int32_t     distanceMm() const { return _distMm; }     // -1: not a distance sensor, or no echo
  const char* type() const { return _type; }
  bool        isDistance() const { return _dist; }

private:
  char     _type[8] = "none";
  bool     _dist = false;
  bool     _present = false;
  int32_t  _distMm = -1;
  int32_t  _hist[3] = {-1, -1, -1};
  uint8_t  _histN = 0;
  uint32_t _nextTrig = 0, _trigAt = 0;
  bool     _waiting = false;
};

extern SensorClass Sensor;
