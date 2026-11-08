#include "FurbyBus.h"
#include "FurbyCodes.h"
#include "FurbyProtocol.h"
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <pico/util/queue.h>

FurbyBusClass FurbyBus;

namespace {

constexpr uint8_t  ACT_DEPTH      = 8;    // queued actions per furby
constexpr uint8_t  CMD_QUEUE_LEN  = 64;   // core0 -> core1 mailbox
constexpr uint16_t SLEEP_RETRY_MS = 800;  // original sketch: 8 x 100 ms between the two sleep messages
constexpr uint16_t SETTLE_MS      = 50;   // wire floats this long after a wake/level pulse: the tilt trigger is
                                          // "a long pulse followed by a floating state" (original sketch notes)

enum CmdKind : uint8_t { CMD_PUSH, CMD_CLEAR, CMD_CLEAR_ALL };
struct Cmd { uint8_t kind; uint8_t ch; Action act; };

queue_t g_q;
ChannelStatus* g_st = nullptr;

// Timing, written by core 0, read by core 1 (single-value reads are atomic on the RP2040)
volatile uint16_t t_pulse = 150;
volatile uint16_t t_half  = 1050;
volatile uint16_t t_gap   = 90;
volatile uint16_t t_wake  = 100;
volatile uint8_t  t_rep   = 6;

struct Chan {
  Action   fifo[ACT_DEPTH];
  uint8_t  head = 0, count = 0;
  bool     running = false;
  Action   cur;
  uint32_t ticks = 0;      // countdown for Wake / Wait / Level
  uint32_t settle = 0;     // forced floating ticks before the next action may start
  uint16_t slot = 0;       // position inside the Msg repeat period
  uint8_t  repeats = 0;
  bool     driven = false; // pad currently configured as output
} g_ch[NUM_FURBIES];

struct Out { bool drive = false, high = false, pulse = false; };

inline uint32_t msToTicks(uint32_t ms) {
  const uint32_t h = t_half;
  const uint32_t t = (ms * 1000UL + h - 1) / h;
  return t ? t : 1;
}

inline uint32_t stamp() { const uint32_t m = millis(); return m ? m : 1; }

void clearChan(uint8_t i) {
  Chan& c = g_ch[i];
  c.head = c.count = 0;
  c.running = false;
  c.settle = 0;
  c.cur = Action();
}

void finishAction(uint8_t i) {
  Chan& c = g_ch[i];
  ChannelStatus& s = g_st[i];
  switch (c.cur.type) {
    case ActType::Wake:
      s.assumed = ASSUME_AWAKE;
      s.lastMs  = stamp();
      c.settle  = msToTicks(SETTLE_MS);
      break;
    case ActType::Level:
      c.settle  = msToTicks(SETTLE_MS);
      break;
    case ActType::Msg:
      s.lastCode = c.cur.a;
      s.lastMs   = stamp();
      if (c.cur.a == CODE_SLEEP || c.cur.a == CODE_BED) s.assumed = ASSUME_ASLEEP;
      break;
    default:
      break;
  }
  c.running = false;
  c.cur = Action();
}

// Advance one furby by one tick and say what its wire should do during this tick.
Out stepChannel(uint8_t i) {
  Chan& c = g_ch[i];
  Out o;

  if (!c.running) {
    if (c.settle) { c.settle--; return o; }   // forced float after a wake / level pulse
    if (c.count == 0) return o;               // idle: wire floats
    c.cur = c.fifo[c.head];
    c.head = (c.head + 1) % ACT_DEPTH;
    c.count--;
    c.running = true;
    c.slot = 0;
    switch (c.cur.type) {
      case ActType::Wake:  c.ticks = msToTicks(c.cur.b ? c.cur.b : t_wake); break;
      case ActType::Wait:  c.ticks = msToTicks(c.cur.b);                    break;
      case ActType::Level: c.ticks = (c.cur.b == 0xFFFF) ? 0xFFFFFFFFUL : msToTicks(c.cur.b); break;
      case ActType::Msg:   c.repeats = c.cur.b ? (uint8_t)c.cur.b : t_rep;  break;
      default: c.running = false; return o;
    }
  }

  switch (c.cur.type) {
    case ActType::Wake:
      o.drive = true; o.high = true;
      if (--c.ticks == 0) finishAction(i);
      break;

    case ActType::Level:
      o.drive = true; o.high = (c.cur.a != 0);
      if (c.ticks != 0xFFFFFFFFUL && --c.ticks == 0) finishAction(i);
      break;

    case ActType::Wait:
      if (--c.ticks == 0) finishAction(i);
      break;

    case ActType::Msg: {
      const uint16_t period = FurbyProtocol::SLOTS_PER_PACKET + msToTicks(t_gap);
      if (c.slot < FurbyProtocol::SLOTS_PER_PACKET) {
        o.drive = true;                                   // low between pulses, like the original
        o.pulse = FurbyProtocol::pulseInSlot(c.cur.a, (uint8_t)c.slot);
      }                                                   // else: floating during the inter-packet gap
      if (++c.slot >= period) {
        c.slot = 0;
        if (--c.repeats == 0) finishAction(i);
      }
      break;
    }
    default:
      c.running = false;
      break;
  }
  return o;
}

uint64_t g_next = 0;

}  // namespace

// ───────────────────────── core 0 ─────────────────────────

void FurbyBusClass::begin(const BusTiming& t) {
  g_st = _st;
  queue_init(&g_q, sizeof(Cmd), CMD_QUEUE_LEN);
  setTiming(t);
  // All wires start (and idle) as floating inputs, no pulls: a furby's tilt/IR
  // input must never be loaded by us while we are not talking.
  for (uint8_t i = 0; i < NUM_FURBIES; i++) {
    const uint8_t pin = FURBY_PIN[i];
    gpio_init(pin);
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
  }
  _ready = true;
}

void FurbyBusClass::setTiming(const BusTiming& t) {
  t_pulse = constrain(t.pulseUs, 5, 500);
  t_half  = constrain(t.halfBitUs, 600, 3000);
  t_gap   = t.gapMs;
  t_wake  = t.wakeMs ? t.wakeMs : 100;
  t_rep   = t.repeats ? t.repeats : 1;
}

void FurbyBusClass::setPulseOnly(uint16_t pulseUs) { t_pulse = constrain(pulseUs, 5, 500); }

bool FurbyBusClass::push(uint8_t ch, const Action& a) {
  if (ch >= NUM_FURBIES || !_ready) return false;
  Cmd c{CMD_PUSH, ch, a};
  return queue_try_add(&g_q, &c);
}

bool FurbyBusClass::wake(uint8_t ch, uint16_t ms) {
  Action a; a.type = ActType::Wake; a.b = ms;
  return push(ch, a);
}

bool FurbyBusClass::send(uint8_t ch, uint8_t code, uint8_t repeats) {
  Action a; a.type = ActType::Msg; a.a = code & 0x0F; a.b = repeats;
  return push(ch, a);
}

bool FurbyBusClass::sleep(uint8_t ch) {
  Action m; m.type = ActType::Msg; m.a = CODE_SLEEP;
  Action w; w.type = ActType::Wait; w.b = SLEEP_RETRY_MS;
  if (!push(ch, m)) return false;
  push(ch, w);
  push(ch, m);
  return true;
}

bool FurbyBusClass::level(uint8_t ch, bool high, uint16_t ms) {
  Action a; a.type = ActType::Level; a.a = high ? 1 : 0; a.b = ms;
  return push(ch, a);
}

void FurbyBusClass::clear(uint8_t ch) {
  if (ch >= NUM_FURBIES || !_ready) return;
  Cmd c{CMD_CLEAR, ch, Action()};
  queue_try_add(&g_q, &c);
}

void FurbyBusClass::clearAll() {
  if (!_ready) return;
  Cmd c{CMD_CLEAR_ALL, 0, Action()};
  queue_try_add(&g_q, &c);
}

// ───────────────────────── core 1 ─────────────────────────

void FurbyBusClass::coreSetup() {
  g_next = time_us_64();
}

void FurbyBusClass::coreTick() {
  _ticks++;
  // 1. take whatever core 0 has queued
  Cmd cmd;
  while (queue_try_remove(&g_q, &cmd)) {
    switch (cmd.kind) {
      case CMD_PUSH: {
        Chan& c = g_ch[cmd.ch];
        if (c.count < ACT_DEPTH) {
          c.fifo[(c.head + c.count) % ACT_DEPTH] = cmd.act;
          c.count++;
        } else {
          _dropped++;
        }
        break;
      }
      case CMD_CLEAR:     clearChan(cmd.ch); break;
      case CMD_CLEAR_ALL: for (uint8_t i = 0; i < NUM_FURBIES; i++) clearChan(i); break;
    }
  }

  // 2. step every furby
  Out out[NUM_FURBIES];
  bool anyPulse = false;
  for (uint8_t i = 0; i < NUM_FURBIES; i++) {
    out[i] = stepChannel(i);
    anyPulse |= out[i].pulse;
  }

  // 3. apply: pulses start now ...
  for (uint8_t i = 0; i < NUM_FURBIES; i++) {
    const uint8_t pin = FURBY_PIN[i];
    Chan& c = g_ch[i];
    if (out[i].drive) {
      gpio_put(pin, out[i].high || out[i].pulse);   // set the level first, then enable the driver: no glitch
      if (!c.driven) { gpio_set_dir(pin, GPIO_OUT); c.driven = true; }
    } else if (c.driven) {
      gpio_set_dir(pin, GPIO_IN);                   // back to floating input
      c.driven = false;
    }
  }
  // ... and end after pulseUs
  if (anyPulse) {
    busy_wait_us_32(t_pulse);
    for (uint8_t i = 0; i < NUM_FURBIES; i++)
      if (out[i].pulse && !out[i].high) gpio_put(FURBY_PIN[i], 0);
  }

  // 4. publish status for core 0
  for (uint8_t i = 0; i < NUM_FURBIES; i++) {
    const Chan& c = g_ch[i];
    _st[i].pending = c.count + (c.running ? 1 : 0);
    _st[i].busy    = c.running || c.count;
  }

  // 5. sleep until the next half-bit slot
  g_next += t_half;
  const uint64_t now = time_us_64();
  if (now > g_next + 2UL * t_half) g_next = now;     // fell behind (e.g. flash write): resync instead of bursting
  while (time_us_64() < g_next) tight_loop_contents();
}
