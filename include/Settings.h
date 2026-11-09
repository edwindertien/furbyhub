#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Hub configuration. Lives in /furbyhub.json on LittleFS and is the single source
// of truth; this struct is its in-RAM form. Everything is editable three ways that
// all go through the same JSON code:
//     CLI   config show | get <path> | set <path> <value>, then `save`
//     web   dashboard > Configuration (GET/POST /api/config)
//     file  upload furbyhub.json with the LittleFS uploader
//
// Passwords are never shown: they read back as "********", and writing "********"
// back keeps the stored value.
// ---------------------------------------------------------------------------

constexpr uint8_t MAX_RULES = 8;

struct FurbyProfile {          // purely cosmetic: how this furby looks in the dashboard
  char name[20];
  char preset[20];             // key in the dashboard's pattern list (leopard, giraffe, ...)
  char eyes[8];                // blue | green | brown | grey | silver | purple
};

struct Rule {                  // "when another hub reports X, do Y"
  char     on[40];             // "hub/3/presence"  or  "hub/3/presence=1"
  char     act[48];            // any CLI line, e.g. "wakeall" or "send 1,2 dance"
  uint16_t delayMs;
};

struct Settings {
  // ── hub ──
  uint8_t  unitId;             // 1..99
  char     hubName[24];
  uint8_t  autoEnabled;
  uint16_t enabledMask;        // bit n-1 = furby n connected (stored as furbies.N.enabled)

  // ── network ──
  char     netMode[5];         // "ap" (own access point) | "sta" (join the router)
  char     staSsid[33];
  char     staPass[64];
  char     apPass[64];         // password of this hub's own access point
  char     ip[16];             // static address in sta mode; empty = DHCP
  char     gateway[16];
  char     mask[16];
  char     oscTarget[16];      // empty = subnet broadcast
  uint16_t oscPort;

  // ── wire protocol ──
  uint16_t pulseUs;
  uint16_t halfBitUs;
  uint16_t gapMs;
  uint16_t wakeMs;
  uint8_t  repeats;

  // ── standalone ──
  uint8_t  chatter;
  uint32_t awakeMs;
  uint16_t wakeStaggerMs;
  uint16_t sleepStaggerMs;
  uint32_t chatMinMs;
  uint32_t chatMaxMs;
  uint32_t cooldownMs;

  // ── sensor ──
  char     sensorType[8];      // none | pir | hcsr04
  uint16_t wakeMm;             // distance sensors: closer than this = someone is there
  uint16_t sleepMm;            // farther than this = gone (>= wakeMm, hysteresis)
  uint16_t smoothMs;           // a reading must hold this long before it counts

  FurbyProfile furby[NUM_FURBIES];
  Rule     rules[MAX_RULES];
  uint8_t  ruleCount;
};

class SettingsStore {
public:
  Settings v;
  String   loadNote;           // what happened at boot: "loaded", "migrated", "defaults", or an error

  void defaults();
  void load();                 // furbyhub.json -> (legacy flash settings) -> defaults
  bool save();                 // atomic: write temp file, then rename
  void resetNetwork();         // back to the AP-mode defaults (button-at-boot recovery)
  void applyRuntime();         // push wire-protocol timing into the pulse engine

  String json(bool redact = true, bool pretty = true) const;
  bool   applyJson(const String& text, String& err, bool& needsReboot);   // validates, applies; does not save
  bool   getPath(const String& path, String& out) const;
  bool   setPath(const String& path, const String& value, String& err, bool& needsReboot);

  bool fsOk() const { return _fsOk; }

private:
  void toDoc(JsonDocument& d, bool redact) const;
  void fromDoc(JsonVariantConst d, String& problems);
  bool applyDoc(JsonVariantConst d, String& err, bool& needsReboot);
  bool _fsOk = false;
};

extern SettingsStore Cfg;
void eepromBegin();            // shared with Diag (idempotent)
