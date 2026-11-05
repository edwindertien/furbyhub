#include "Settings.h"
#include <EEPROM.h>

SettingsStore Cfg;

static constexpr uint32_t MAGIC   = 0x46555242;  // 'FURB'
static constexpr uint16_t VERSION = 1;

static uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*p++) << 8;
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

void SettingsStore::defaults() {
  memset(&v, 0, sizeof(v));
  v.magic          = MAGIC;
  v.version        = VERSION;
  v.unitId         = 1;
  v.autoEnabled    = 1;
  v.enabledMask    = 0xFFFF;

  v.pulseUs        = 150;
  v.halfBitUs      = 1050;
  v.gapMs          = 90;
  v.wakeMs         = 100;
  v.repeats        = 6;
  v.chatter        = 1;

  v.awakeMs        = 60000;   // the original sketch kept furbies awake for 60 s
  v.wakeStaggerMs  = 300;
  v.sleepStaggerMs = 150;
  v.chatMinMs      = 6000;
  v.chatMaxMs      = 20000;
  v.cooldownMs     = 3000;
}

void SettingsStore::load() {
  EEPROM.begin(256);
  Settings tmp;
  EEPROM.get(0, tmp);
  const uint16_t want = crc16((const uint8_t*)&tmp, sizeof(Settings) - sizeof(uint16_t));
  if (tmp.magic == MAGIC && tmp.version == VERSION && tmp.crc == want) {
    v = tmp;
  } else {
    defaults();
  }
}

bool SettingsStore::save() {
  v.crc = crc16((const uint8_t*)&v, sizeof(Settings) - sizeof(uint16_t));
  EEPROM.put(0, v);
  return EEPROM.commit();
}
