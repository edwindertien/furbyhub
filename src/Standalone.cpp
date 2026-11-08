#include "Standalone.h"
#include "FurbyBus.h"
#include "FurbyCodes.h"
#include "Settings.h"
#include "Diag.h"

StandaloneClass Standalone;

namespace {
inline bool due(uint32_t t) { return (int32_t)(millis() - t) >= 0; }   // wrap-safe
}

void StandaloneClass::begin() {
  pinMode(PIN_SENSOR, SENSOR_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);  // unplugged sensor = inactive
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  _state = Idle;
  _cooldownUntil = millis() + 2000;   // let sensors settle after power-up (PIRs often fire once on boot)
}

const char* StandaloneClass::stateName() const {
  switch (_state) {
    case Idle:     return "idle";
    case Waking:   return "waking";
    case Awake:    return "awake";
    case Sleeping: return "sleeping";
  }
  return "?";
}

uint32_t StandaloneClass::remainingMs() const {
  if (_state != Awake || !Cfg.v.autoEnabled) return 0;
  const int32_t left = (int32_t)(_awakeUntil - millis());
  return left > 0 ? (uint32_t)left : 0;
}

int StandaloneClass::nextEnabled(int from) const {
  for (int i = from; i < NUM_FURBIES; i++)
    if (Cfg.v.enabledMask & (1u << i)) return i;
  return -1;
}

// ── actions ─────────────────────────────────────────────────────────────────

void StandaloneClass::wakeAll() {
  _state  = Waking;
  _idx    = 0;
  _nextAt = millis();
}

void StandaloneClass::sleepAll() {
  _state  = Sleeping;
  _idx    = 0;
  _nextAt = millis();
}

void StandaloneClass::trigger() {
  if (_state == Idle) {
    if (due(_cooldownUntil)) wakeAll();
  } else if (_state == Awake && Cfg.v.autoEnabled) {
    _awakeUntil = millis() + Cfg.v.awakeMs;
  }
}

void StandaloneClass::scheduleChat(uint32_t minDelay) {
  const uint32_t lo = Cfg.v.chatMinMs, hi = max<uint32_t>(Cfg.v.chatMaxMs, lo + 1);
  _nextChat = millis() + minDelay + random(lo, hi);
}

// ── state machine ───────────────────────────────────────────────────────────

void StandaloneClass::stepWaking() {
  if (!due(_nextAt)) return;
  const int i = nextEnabled(_idx);
  if (i < 0) {
    _state      = Awake;
    _awakeUntil = millis() + Cfg.v.awakeMs;
    scheduleChat(6000);               // a freshly woken furby is busy talking and ignores packets
    return;
  }
  FurbyBus.wake(i);
  _idx    = i + 1;
  _nextAt = millis() + Cfg.v.wakeStaggerMs;
}

void StandaloneClass::stepSleeping() {
  if (!due(_nextAt)) return;
  const int i = nextEnabled(_idx);
  if (i < 0) {
    _state         = Idle;
    _cooldownUntil = millis() + Cfg.v.cooldownMs;
    return;
  }
  FurbyBus.sleep(i);
  _idx    = i + 1;
  _nextAt = millis() + Cfg.v.sleepStaggerMs;
}

void StandaloneClass::stepAwake() {
  if (!Cfg.v.autoEnabled) return;     // manual mode: no timeout, no chatter

  if (_sensor) _awakeUntil = millis() + Cfg.v.awakeMs;   // somebody is still there
  if (due(_awakeUntil)) { sleepAll(); return; }

  if (Cfg.v.chatter && due(_nextChat)) {
    int cand[NUM_FURBIES], n = 0;
    for (int i = 0; i < NUM_FURBIES; i++) {
      if (!(Cfg.v.enabledMask & (1u << i))) continue;
      const ChannelStatus& s = FurbyBus.status(i);
      if (s.assumed == ASSUME_AWAKE && !s.busy) cand[n++] = i;
    }
    if (n) {
      const int who = cand[random(n)];
      FurbyBus.send(who, CHATTER_CODES[random(CHATTER_CODES_LEN)]);
    }
    scheduleChat();
  }
}

void StandaloneClass::readInputs() {
  const uint32_t now = millis();

  const bool s = (digitalRead(PIN_SENSOR) == HIGH) == SENSOR_ACTIVE_HIGH;
  if (s != _sensorRaw) { _sensorRaw = s; _sensorSince = now; }
  if (s != _sensor && now - _sensorSince >= INPUT_DEBOUNCE_MS) {
    _sensor = s;
    if (_sensor && Cfg.v.autoEnabled) trigger();
  }

  const bool b = (digitalRead(PIN_BUTTON) == LOW);
  if (b != _buttonRaw) { _buttonRaw = b; _buttonSince = now; }
  if (b != _button && now - _buttonSince >= INPUT_DEBOUNCE_MS) {
    _button = b;
    if (_button) {                    // physical override, works in auto and manual mode
      if (_state == Idle)       wakeAll();
      else if (_state == Awake) sleepAll();
    }
  }
}

void StandaloneClass::loop() {
  if (!(Dg.flags & DIAG_NO_INPUTS)) readInputs();
  switch (_state) {
    case Waking:   stepWaking();   break;
    case Sleeping: stepSleeping(); break;
    case Awake:    stepAwake();    break;
    case Idle:     break;
  }
}
