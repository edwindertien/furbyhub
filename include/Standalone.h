#pragma once
#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Standalone behaviour of one unit - works with or without any WiFi client.
//
//   Idle ──trigger──▶ Waking ──all woken──▶ Awake ──timeout──▶ Sleeping ──▶ Idle
//
//   trigger   = presence sensor going active (only when `auto` is on), the
//               physical button, or "wake everyone" on the dashboard / CLI
//   Waking    = furbies are woken one by one, `wakeStaggerMs` apart, so sixteen
//               motors do not all start at once on the shared supply
//   Awake     = while `auto` is on: stays awake as long as the sensor keeps
//               seeing somebody, plus `awakeMs`; furbies may chat at random
//   Sleeping  = every furby gets the sleep message, `sleepStaggerMs` apart
//
// With `auto` off nothing happens by itself: no sensor reaction, no timeout,
// no chatter. Manual wake/sleep (dashboard, CLI, button) always works.
// ---------------------------------------------------------------------------
class StandaloneClass {
public:
  enum State : uint8_t { Idle, Waking, Awake, Sleeping };

  void begin();
  void loop();                 // call every loop()

  void wakeAll();              // staggered, enabled furbies only
  void sleepAll();             // staggered, enabled furbies only
  void trigger();              // what the sensor does

  State       state() const { return _state; }
  const char* stateName() const;
  uint32_t    remainingMs() const;   // time until automatic sleep, 0 when not applicable
  bool        sensorActive() const { return _sensor; }
  bool        buttonDown()   const { return _button; }

private:
  void readInputs();
  void stepWaking();
  void stepSleeping();
  void stepAwake();
  void scheduleChat(uint32_t minDelay = 0);
  int  nextEnabled(int from) const;

  State    _state = Idle;
  int      _idx = 0;
  uint32_t _nextAt = 0;
  uint32_t _awakeUntil = 0;
  uint32_t _cooldownUntil = 0;
  uint32_t _nextChat = 0;

  bool     _sensor = false, _sensorRaw = false;
  uint32_t _sensorSince = 0;
  bool     _button = false, _buttonRaw = false;
  uint32_t _buttonSince = 0;
};

extern StandaloneClass Standalone;
