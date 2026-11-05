#pragma once
#include <Arduino.h>
#include "config.h"

// Everything that survives a power cycle. Stored in emulated EEPROM (flash).
struct Settings {
  uint32_t magic;
  uint16_t version;
  uint8_t  unitId;          // 1..99, decides the SSID
  uint8_t  autoEnabled;     // standalone behaviour on/off
  uint16_t enabledMask;     // bit n-1 set = furby n is connected / used

  // ── wire protocol ──
  uint16_t pulseUs;         // width of one pulse                       (orig. 150)
  uint16_t halfBitUs;       // half a bit time = engine tick            (orig. ~1050)
  uint16_t gapMs;           // silence between the repeated packets      (orig. 90)
  uint16_t wakeMs;          // length of the long "tilt" pulse           (orig. 100)
  uint8_t  repeats;         // packet repetitions per message            (orig. 6)
  uint8_t  chatter;         // standalone: random chatter while awake

  // ── standalone behaviour ──
  uint32_t awakeMs;         // how long furbies stay awake after the last trigger
  uint16_t wakeStaggerMs;   // delay between waking furby n and n+1 (limits inrush current)
  uint16_t sleepStaggerMs;
  uint16_t chatMinMs;       // random chatter interval
  uint16_t chatMaxMs;
  uint16_t cooldownMs;      // ignore triggers for this long after going to sleep

  uint16_t crc;             // must stay last
};

class SettingsStore {
public:
  Settings v;
  void defaults();
  void load();              // falls back to defaults() on bad magic/crc
  bool save();
};

extern SettingsStore Cfg;
