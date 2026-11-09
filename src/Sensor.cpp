#include "Sensor.h"
#include "SensorLogic.h"
#include "Settings.h"
#include "config.h"
#include <hardware/gpio.h>

SensorClass Sensor;

namespace {
SensorLogic::Presence g_logic;

// ultrasonic echo timing, measured in an interrupt so loop() never blocks
volatile uint32_t g_rise = 0, g_width = 0;
volatile bool     g_have = false;

void echoIsr() {
  if (gpio_get(PIN_ECHO)) g_rise = micros();
  else { g_width = micros() - g_rise; g_have = true; }
}

constexpr uint32_t TRIG_PERIOD_MS = 60;     // ~16 readings per second
constexpr uint32_t ECHO_TIMEOUT_MS = 35;    // 35 ms ~ 6 m: nothing came back
}  // namespace

void SensorClass::begin() {
  strlcpy(_type, Cfg.v.sensorType, sizeof _type);
  _dist = !strcmp(_type, "hcsr04");

  if (!strcmp(_type, "pir")) {
    pinMode(PIN_SENSOR, SENSOR_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);   // unplugged sensor = inactive
  } else if (_dist) {
    pinMode(PIN_TRIG, OUTPUT);
    digitalWrite(PIN_TRIG, LOW);
    pinMode(PIN_ECHO, INPUT_PULLDOWN);
    attachInterrupt(PIN_ECHO, echoIsr, CHANGE);
  }
  _nextTrig = millis() + 200;
}

void SensorClass::loop() {
  const uint32_t now = millis();

  if (!strcmp(_type, "pir")) {
    const bool raw = (digitalRead(PIN_SENSOR) == HIGH) == SENSOR_ACTIVE_HIGH;
    // reuse the same hold-time logic: active = "0 mm", idle = "far"
    _present = g_logic.update(raw ? 0 : 9999, now, 1, 2, INPUT_DEBOUNCE_MS);
    return;
  }
  if (!_dist) { _present = false; return; }

  // ── ultrasonic: collect the last echo, then fire the next trigger ──
  if (_waiting) {
    int32_t mm = -1;
    bool done = false;
    if (g_have) {
      const uint32_t w = g_width;
      const int32_t d = (int32_t)((uint64_t)w * 343 / 2000);     // 343 m/s, there and back
      mm = (d >= 20 && d <= 4000) ? d : -1;
      done = true;
    } else if ((int32_t)(now - _trigAt) > (int32_t)ECHO_TIMEOUT_MS) {
      done = true;                                               // no echo
    }
    if (done) {
      _waiting = false;
      _hist[_histN++ % 3] = mm;
      _distMm = SensorLogic::median3(_hist[0], _hist[1], _hist[2]);
      _present = g_logic.update(_distMm, now, Cfg.v.wakeMm, Cfg.v.sleepMm, Cfg.v.smoothMs);
    }
  }
  if (!_waiting && (int32_t)(now - _nextTrig) >= 0) {
    g_have = false;
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    _trigAt = now;
    _nextTrig = now + TRIG_PERIOD_MS;
    _waiting = true;
  }
}
