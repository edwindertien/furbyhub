#include "Diag.h"
#include <EEPROM.h>
#include <hardware/watchdog.h>
#include <stdarg.h>

DiagState Dg;

static constexpr int      DIAG_ADDR  = 160;       // Settings lives at 0..~60 of the 256-byte EEPROM
static constexpr uint16_t DIAG_MAGIC = 0xD1A6;
static constexpr uint32_t CRUMB_TAG  = 0xC0DE0000;

struct DiagStore { uint16_t magic; uint8_t flags; uint8_t channel; };

void diagLoad() {
  DiagStore s;
  EEPROM.get(DIAG_ADDR, s);
  if (s.magic == DIAG_MAGIC) { Dg.flags = s.flags; Dg.channel = s.channel; }
  else                       { Dg.flags = 0;       Dg.channel = 0; }
  Dg.core1Enabled = !(Dg.flags & DIAG_NO_CORE1);
}

bool diagSave() {
  DiagStore s{DIAG_MAGIC, Dg.flags, Dg.channel};
  EEPROM.put(DIAG_ADDR, s);
  return EEPROM.commit();
}

void diagCaptureReset() {
  const uint32_t c = watchdog_hw->scratch[2];
  Dg.resetWatchdog = watchdog_caused_reboot();
  Dg.resetStage = ((c & 0xFFFF0000u) == CRUMB_TAG) ? (uint8_t)(c & 0xFF) : (uint8_t)CR_NONE;
  watchdog_hw->scratch[2] = 0;
}

void crumb(uint8_t stage) { watchdog_hw->scratch[2] = CRUMB_TAG | stage; }

void dlog(const char* fmt, ...) {
  if (Dg.flags & DIAG_QUIET) return;
  char buf[160];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  const uint32_t ms = millis();
  Serial.printf("[%lu.%03lu] %s\r\n", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000), buf);
}

const char* crumbName(uint8_t s) {
  switch (s) {
    case CR_NONE: return "none";           case CR_LOOP: return "main loop";
    case CR_CLI: return "cli";             case CR_STANDALONE: return "standalone";
    case CR_DNS: return "dns";             case CR_ACCEPT: return "http accept";
    case CR_READ_REQ: return "http read request line";
    case CR_READ_HDR: return "http read headers";
    case CR_SEND: return "http send";      case CR_CLOSE: return "http flush/close";
    case CR_STATIONS: return "station poll";
    case CR_AP_START: return "AP start";   case CR_AP_RESTART: return "AP restart";
    case CR_FLASH_SAVE: return "flash save";
    case CR_CLEAN_REBOOT: return "commanded reboot";
  }
  return "?";
}

struct FlagName { const char* n; uint8_t bit; };
static const FlagName FLAGS[] = {
  {"core1", DIAG_NO_CORE1}, {"wdt", DIAG_NO_WDT}, {"led", DIAG_NO_LED},
  {"stationpoll", DIAG_NO_STATIONPOLL}, {"inputs", DIAG_NO_INPUTS}, {"dns", DIAG_NO_DNS},
};

bool diagFlagByName(const String& name, uint8_t& bit) {
  for (auto& f : FLAGS) if (name == f.n) { bit = f.bit; return true; }
  return false;
}

void diagPrintFlags(Print& out) {
  out.print("subsystems: ");
  for (auto& f : FLAGS) { out.print(f.n); out.print((Dg.flags & f.bit) ? "=OFF " : "=on "); }
  out.printf("  log=%s  wifi channel=%s\r\n", (Dg.flags & DIAG_QUIET) ? "quiet" : "verbose",
             Dg.channel ? String(Dg.channel).c_str() : "default");
}
