#pragma once
// ---------------------------------------------------------------------------
// Diagnostics for bringing up the WiFi side.
//
//  * dlog()      timestamped log lines on USB serial (toggle with `debug on|off`)
//  * crumb()     remembers the last code stage in a watchdog scratch register, which
//                survives a watchdog reset - so after a crash `net` can tell where it hung
//  * switches    individual subsystems can be disabled from the CLI (`diag off core1 ...`),
//                stored in flash, applied at the next boot. Used to bisect "why does the
//                AP drop my phone" without reflashing.
// ---------------------------------------------------------------------------
#include <Arduino.h>

enum : uint8_t {
  DIAG_NO_CORE1       = 1 << 0,   // furby pulse engine on core 1 (also stops all furby output!)
  DIAG_NO_WDT         = 1 << 1,   // hardware watchdog
  DIAG_NO_LED         = 1 << 2,   // status LED (lives on the WiFi chip)
  DIAG_NO_STATIONPOLL = 1 << 3,   // periodic softAPgetStationNum()
  DIAG_NO_INPUTS      = 1 << 4,   // presence sensor + button polling
  DIAG_NO_DNS         = 1 << 5,   // captive-portal DNS server
  DIAG_QUIET          = 1 << 6,   // no live log lines
};

// crumb stages
enum : uint8_t {
  CR_NONE = 0, CR_LOOP = 1, CR_CLI = 2, CR_STANDALONE = 3,
  CR_DNS = 10, CR_ACCEPT = 11, CR_READ_REQ = 12, CR_READ_HDR = 13, CR_SEND = 14, CR_CLOSE = 15,
  CR_STATIONS = 20, CR_AP_START = 21, CR_AP_RESTART = 22,
  CR_FLASH_SAVE = 30, CR_OSC = 40, CR_CLEAN_REBOOT = 250,
};

struct DiagState {
  uint8_t  flags = 0;             // persisted
  uint8_t  channel = 0;           // persisted, 0 = default per unit
  bool     core1Enabled = true;   // runtime copy read by core 1
  bool     wdtOn = false;         // watchdog running (set by main once it starts)
  // reset information captured at boot
  bool     resetWatchdog = false;
  uint8_t  resetStage = CR_NONE;
  // counters
  uint32_t httpRequests = 0, httpEmpty = 0, httpFailedSend = 0;
};
extern DiagState Dg;

void diagLoad();                  // call after Cfg.load() (EEPROM must be begun)
bool diagSave();
void diagCaptureReset();          // call once, first thing in setup()
void crumb(uint8_t stage);
void dlog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
const char* crumbName(uint8_t stage);
bool diagFlagByName(const String& name, uint8_t& bit);
void diagPrintFlags(Print& out);
