#pragma once
// ---------------------------------------------------------------------------
// FurbyBus - drives up to 16 furby signal wires, all in parallel.
//
// Runs on core 1 (setup1()/loop1() in main.cpp) so that WiFi, HTTP and the CLI
// on core 0 can never delay a pulse. Core 0 talks to it through a lock-free
// queue of small "actions" per furby:
//
//   Wake   long HIGH pulse (fakes the tilt sensor), then the pin floats
//   Msg    one 4-bit message, repeated N times (the IR-style packet)
//   Wait   pause between actions on the same furby
//   Level  drive the wire HIGH or LOW for a while (bench testing), then float
//
// A wire is only driven while something is happening on it; otherwise it is a
// high-impedance input, exactly like the original sketch (pinMode(INPUT)), so
// it cannot disturb the furby's own tilt sensor or IR receiver.
//
// Timing: one engine tick = half a bit time (default 1050 us). Every furby is
// stepped each tick, so 16 furbies cost the same as one.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include "config.h"

enum class ActType : uint8_t { None = 0, Wake, Msg, Wait, Level };

struct Action {
  ActType  type = ActType::None;
  uint8_t  a    = 0;     // Msg: code (0..15)   Level: 1 = HIGH, 0 = LOW
  uint16_t b    = 0;     // Wake/Wait/Level: ms (0 = default)   Msg: repeats (0 = default)
};

// What core 0 may know about a furby. Furbies cannot talk back over this wire, so
// "assumed" is only what we last told it - not a measurement.
enum : uint8_t { ASSUME_UNKNOWN = 0, ASSUME_ASLEEP = 1, ASSUME_AWAKE = 2 };

struct ChannelStatus {
  volatile uint8_t  assumed  = ASSUME_UNKNOWN;
  volatile uint8_t  lastCode = 0xFF;   // last message code sent (0xFF = none)
  volatile uint8_t  pending  = 0;      // queued + running actions
  volatile bool     busy     = false;
  volatile uint32_t lastMs   = 0;      // millis() of last completed action (0 = never)
};

struct BusTiming {
  uint16_t pulseUs;
  uint16_t halfBitUs;
  uint16_t gapMs;
  uint16_t wakeMs;
  uint8_t  repeats;
};

class FurbyBusClass {
public:
  // ── core 0 ──
  void begin(const BusTiming& t);                 // initialises pins + queue, then releases core 1
  void setTiming(const BusTiming& t);
  void setPulseOnly(uint16_t pulseUs);            // temporary override (sweep tests), not persisted
  bool ready() const { return _ready; }

  bool push(uint8_t ch, const Action& a);         // ch = 0-based
  bool wake(uint8_t ch, uint16_t ms = 0);
  bool send(uint8_t ch, uint8_t code, uint8_t repeats = 0);
  bool sleep(uint8_t ch);                         // send "sleep", wait, send it again (as the original sketch did)
  bool level(uint8_t ch, bool high, uint16_t ms); // ms = 0xFFFF holds until clear()
  void clear(uint8_t ch);                         // drop everything queued, float the wire
  void clearAll();

  const ChannelStatus& status(uint8_t ch) const { return _st[ch]; }
  uint32_t dropped() const { return _dropped; }   // actions lost to a full queue

  // ── core 1 ──
  void coreSetup();
  void coreTick();                                // call in a loop; blocks until the next tick is due

private:
  volatile bool _ready = false;
  volatile uint32_t _dropped = 0;
  ChannelStatus _st[NUM_FURBIES];
};

extern FurbyBusClass FurbyBus;
