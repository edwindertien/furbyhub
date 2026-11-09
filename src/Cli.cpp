#include "Cli.h"
#include "config.h"
#include "FurbyBus.h"
#include "FurbyCodes.h"
#include "FurbyProtocol.h"
#include "Settings.h"
#include "Standalone.h"
#include "Dashboard.h"
#include <hardware/watchdog.h>
#include "Diag.h"
#include "Sensor.h"
#include "Osc.h"

CliClass Cli;

// ───────────────────────── helpers ─────────────────────────
namespace {

constexpr size_t LOG_MAX = 1800;

int tokenize(const String& line, String* tok, int maxTok) {
  int n = 0, i = 0, len = line.length();
  while (i < len && n < maxTok) {
    while (i < len && line[i] == ' ') i++;
    if (i >= len) break;
    int j = i;
    while (j < len && line[j] != ' ') j++;
    tok[n++] = line.substring(i, j);
    i = j;
  }
  return n;
}

// the rest of the line after skipping `skip` space-separated tokens (keeps inner spaces)
String restOf(const String& line, int skip) {
  int i = 0, len = line.length();
  for (int k = 0; k < skip; k++) {
    while (i < len && line[i] == ' ') i++;
    while (i < len && line[i] != ' ') i++;
  }
  while (i < len && line[i] == ' ') i++;
  return line.substring(i);
}

bool isNum(const String& s) {
  if (!s.length()) return false;
  for (unsigned i = 0; i < s.length(); i++) if (!isDigit(s[i])) return false;
  return true;
}

// "all" | "3" | "1,4,7" | "2-5" | "1-3,9"   (1-based; "all" = every *enabled* furby)
bool parseSet(String s, uint16_t& mask) {
  s.trim(); s.toLowerCase(); mask = 0;
  if (s == "all") { mask = Cfg.v.enabledMask; return mask != 0; }
  int start = 0, len = s.length();
  while (start < len) {
    int comma = s.indexOf(',', start);
    if (comma < 0) comma = len;
    String part = s.substring(start, comma);
    int dash = part.indexOf('-');
    int a, b;
    if (dash < 0) {
      if (!isNum(part)) return false;
      a = b = part.toInt();
    } else {
      String p1 = part.substring(0, dash), p2 = part.substring(dash + 1);
      if (!isNum(p1) || !isNum(p2)) return false;
      a = p1.toInt(); b = p2.toInt();
    }
    if (a < 1 || b > NUM_FURBIES || a > b) return false;
    for (int i = a; i <= b; i++) mask |= 1u << (i - 1);
    start = comma + 1;
  }
  return mask != 0;
}

// a..p | 0..15 | 0x0..0xF | name | dance | sneeze | sheep | bed | sleep
bool parseCode(String s, uint8_t& code) {
  s.trim(); s.toLowerCase();
  if (s.length() == 1 && s[0] >= 'a' && s[0] <= 'p') { code = s[0] - 'a'; return true; }
  for (uint8_t i = 0; i < CODE_TABLE_LEN; i++)
    if (s == CODE_TABLE[i].key) { code = CODE_TABLE[i].code; return true; }
  char* end = nullptr;
  long v = strtol(s.c_str(), &end, 0);
  if (end && *end == 0 && s.length() && v >= 0 && v <= 15) { code = (uint8_t)v; return true; }
  return false;
}

void printSet(Print& out, uint16_t mask) {
  bool first = true;
  for (int i = 0; i < NUM_FURBIES; i++) {
    if (!(mask & (1u << i))) continue;
    if (!first) out.print(',');
    out.print(i + 1);
    first = false;
  }
  if (first) out.print("none");
}

bool onOff(const String& s, bool& v) {
  if (s == "on" || s == "1" || s == "yes" || s == "true")  { v = true;  return true; }
  if (s == "off" || s == "0" || s == "no" || s == "false") { v = false; return true; }
  return false;
}

// Output sink for scripts: shows on USB serial *and* is kept in a log the dashboard can read.
class TeeLog : public Print {
public:
  TeeLog(String& log) : _log(log) {}
  size_t write(uint8_t c) override {
    Serial.write(c);
    _log += (char)c;
    if (_log.length() > LOG_MAX) _log.remove(0, _log.length() - LOG_MAX);
    return 1;
  }
private:
  String& _log;
};

}  // namespace

// ───────────────────────── setup / poll ─────────────────────────

void CliClass::begin() {
  _rx.reserve(96);
  _log.reserve(LOG_MAX + 64);
  Serial.println();
  Serial.printf("%s %s - unit %u. Type 'help'.\r\n", FW_NAME, FW_VERSION, Cfg.v.unitId);
}

void CliClass::applyTiming() { Cfg.applyRuntime(); }

void CliClass::poll() {
  while (Serial.available()) {
    const char ch = (char)Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (_rx.length()) {
        String line = _rx;
        _rx = "";
        execute(line, Serial);
      }
    } else if (ch == 0x08 || ch == 0x7F) {
      if (_rx.length()) _rx.remove(_rx.length() - 1);
    } else if (_rx.length() < 90) {
      _rx += ch;
    }
  }

  if (_watchUntil) {
    if ((int32_t)(millis() - _watchUntil) >= 0) { _watchUntil = 0; Serial.println("watch finished"); }
    else if ((int32_t)(millis() - _watchNext) >= 0) {
      _watchNext = millis() + 250;
      Serial.printf("%s  dist=%ld mm  %s\r\n", Sensor.type(), (long)Sensor.distanceMm(), Sensor.present() ? "PRESENT" : "-");
    }
  }

  if (_scriptRunning && (int32_t)(millis() - _scriptDue) >= 0) {
    TeeLog tee(_log);
    const Step& st = _script[_scriptPos];
    tee.print("> "); tee.println(st.line);
    execute(st.line, tee);
    _scriptPos++;
    if (_scriptPos >= _script.size()) {
      _scriptRunning = false;
      tee.println("script done");
    } else {
      _scriptDue = millis() + _script[_scriptPos].afterMs;
    }
  }
}

void CliClass::startScript(std::vector<Step>& steps, Print& out) {
  if (_scriptRunning) { out.println("a script is already running - 'stop' first"); return; }
  if (steps.empty()) return;
  _script = steps;
  _scriptPos = 0;
  _scriptDue = millis() + _script[0].afterMs;
  _scriptRunning = true;
  _log = "";
  out.printf("script started (%u steps). Progress is shown here / use 'log'.\r\n", (unsigned)_script.size());
}

void CliClass::stopScript() {
  _scriptRunning = false;
  _script.clear();
}

// ───────────────────────── status / help ─────────────────────────

void CliClass::printStatus(Print& out) {
  const Settings& v = Cfg.v;
  out.printf("%s %s  unit %u  SSID %s%u\r\n", FW_NAME, FW_VERSION, v.unitId, WIFI_SSID_PREFIX, v.unitId);
  {
    char rs[64];
    if (Dg.resetWatchdog && Dg.resetStage == CR_CLEAN_REBOOT) strcpy(rs, "commanded reboot");
    else if (Dg.resetWatchdog) snprintf(rs, sizeof rs, "WATCHDOG, last stage: %s", crumbName(Dg.resetStage));
    else strcpy(rs, "power-on / other");
    out.printf("uptime: %lus   last reset: %s   wifi AP: %s\r\n", (unsigned long)(millis() / 1000), rs, Dashboard.apUp() ? "up" : "DOWN");
  }
  out.printf("config: %s\r\n", Cfg.loadNote.c_str());
  out.printf("auto: %s   standalone: %s", v.autoEnabled ? "on" : "off", Standalone.stateName());
  if (Standalone.remainingMs()) out.printf(" (sleeps in %lu s)", (unsigned long)(Standalone.remainingMs() / 1000));
  out.printf("   sensor: %s   button: %s   wifi clients: %u\r\n",
             Standalone.sensorActive() ? "ACTIVE" : "idle", Standalone.buttonDown() ? "down" : "up", Dashboard.stations());
  out.printf("timing: pulse=%uus  half-bit=%uus (bit=%uus)  repeats=%u  gap=%ums  wake=%ums\r\n",
             v.pulseUs, v.halfBitUs, v.halfBitUs * 2, v.repeats, v.gapMs, v.wakeMs);
  out.printf("standalone: awake=%lus  wakestagger=%ums  sleepstagger=%ums  chatter=%s (%u-%us)  cooldown=%us\r\n",
             (unsigned long)(v.awakeMs / 1000), v.wakeStaggerMs, v.sleepStaggerMs, v.chatter ? "on" : "off",
             (unsigned)(v.chatMinMs / 1000), (unsigned)(v.chatMaxMs / 1000), (unsigned)(v.cooldownMs / 1000));
  out.print("enabled furbies: "); printSet(out, v.enabledMask); out.println();
  out.println("  #  gpio  en  assumed   last message        queue");
  for (int i = 0; i < NUM_FURBIES; i++) {
    const ChannelStatus& s = FurbyBus.status(i);
    const char* st = s.assumed == ASSUME_AWAKE ? "awake  " : s.assumed == ASSUME_ASLEEP ? "asleep " : "unknown";
    char last[24];
    if (!s.lastMs) strcpy(last, "-");
    else if (s.lastCode == 0xFF) snprintf(last, sizeof last, "wake %lus ago", (unsigned long)(max<int32_t>(0, (int32_t)(millis() - s.lastMs)) / 1000));
    else snprintf(last, sizeof last, "%c(%u) %lus ago", FurbyProtocol::letterFor(s.lastCode), s.lastCode, (unsigned long)(max<int32_t>(0, (int32_t)(millis() - s.lastMs)) / 1000));
    out.printf(" %2d  GP%-2u   %c   %s   %-18s  %u\r\n", i + 1, FURBY_PIN[i],
               (v.enabledMask & (1u << i)) ? 'y' : '-', st, last, s.pending);
  }
  if (FurbyBus.dropped()) out.printf("warning: %lu actions dropped (queue full)\r\n", (unsigned long)FurbyBus.dropped());
}

void CliClass::printHelp(Print& out) {
  out.println(
    "<set> = all | 3 | 1,4,7 | 2-5      (1-based; 'all' = enabled furbies)\r\n"
    "<code>= a..p | 0..15 | name dance sneeze sheep bed sleep\r\n"
    "\r\n"
    "wake <set>                  long pulse (acts like the tilt sensor)\r\n"
    "sleep <set>                 sleep message, sent twice like the original\r\n"
    "send <set> <code> [reps]    one message, repeated 'reps' times\r\n"
    "wakeall | sleepall          staggered, enabled furbies (same as dashboard)\r\n"
    "trigger                     act as if the presence sensor fired\r\n"
    "\r\n"
    "bench tests (3.3V hack):\r\n"
    "pulse <set> <ms>            HIGH for ms, then float\r\n"
    "hold <set> high|low|float [ms]   drive the wire; no ms = until 'hold .. float' / 'stop'\r\n"
    "sweep <n> [from [to [step]]]     wake furby n, then send 'name' at pulse widths from..to us\r\n"
    "selftest <set>              wake, name, dance, sleep - sequenced\r\n"
    "stop                        abort scripts, clear all queues, release all wires\r\n"
    "log                         output of the last test script\r\n"
    "\r\n"
    "config:\r\n"
    "status | sensor | pins\r\n"
    "enable <set> | disable <set>\r\n"
    "auto on|off                 standalone behaviour (sensor, timeout, chatter)\r\n"
    "timing [pulse|bit|half|repeats|gap|wake <value>]\r\n"
    "set awake|chatmin|chatmax|cooldown <seconds>\r\n"
    "set wakestagger|sleepstagger <ms>      set chatter on|off\r\n"
    "unit <1..99>                SSID number (applies after reboot)\r\n"
    "save | defaults | reboot\r\n"
    "\r\n"
    "WiFi diagnostics:\r\n"
    "net                         network state, http counters, heap, core 1 alive?, last reset cause\r\n"
    "osc status | osc send /address [args...] | peers      hub-to-hub OSC\r\n"
    "sensor [watch [s]]         type, live distance, thresholds; watch streams readings\r\n"
    "debug on|off                live log of every connection/request on this console\r\n"
    "diag                        show subsystem switches\r\n"
    "diag off|on <core1 wdt led stationpoll inputs dns ...>   (applies after reboot)\r\n"
    "diag channel <0|1..13>      force WiFi channel (0 = default)   |   diag reset\r\n"
    "('timing' and 'set' change RAM only - use 'save' to keep them.)\r\n"
    "\r\n"
    "furbyhub.json (everything above is also reachable as a JSON path):\r\n"
    "config show                 whole configuration (passwords hidden)\r\n"
    "config get <path>           e.g. network.ssid, furbies.3.preset, sensor\r\n"
    "config set <path> <value>   RAM only until 'save'; furbies.N.* use furby numbers, rules.N.* are 0-based\r\n"
    "config reload               re-read the file from flash\r\n"
    "rule list | rule add <on> <delay_ms> <command...> | rule del <n> | rule clear\r\n"
    "                            e.g. rule add hub/3/presence=1 400 wakeall");
}

// ───────────────────────── command dispatch ─────────────────────────

void CliClass::execute(const String& lineIn, Print& out) {
  String line = lineIn;
  line.trim();
  if (!line.length()) return;

  String t[6];
  const int n = tokenize(line, t, 6);
  String cmd = t[0];
  cmd.toLowerCase();
  Settings& v = Cfg.v;

  auto needSet = [&](int idx, uint16_t& mask) -> bool {
    if (n <= idx || !parseSet(t[idx], mask)) { out.println("need a furby set, e.g. 3, 1,4,7, 2-5 or all (try 'help')"); return false; }
    return true;
  };
  auto forSet = [&](uint16_t mask, auto fn) { for (int i = 0; i < NUM_FURBIES; i++) if (mask & (1u << i)) fn(i); };

  if (cmd == "help" || cmd == "?") { printHelp(out); return; }
  if (cmd == "status" || cmd == "st") { printStatus(out); return; }
  if (cmd == "version") { out.printf("%s %s\r\n", FW_NAME, FW_VERSION); return; }

  if (cmd == "pins") {
    for (int i = 0; i < NUM_FURBIES; i++) out.printf("furby %2d = GP%u\r\n", i + 1, FURBY_PIN[i]);
    out.printf("sensor = GP%u (active %s)   button = GP%u (to GND)\r\n", PIN_SENSOR, SENSOR_ACTIVE_HIGH ? "high" : "low", PIN_BUTTON);
    return;
  }

  if (cmd == "sensor") {
    if (n >= 2 && t[1] == "watch") {                       // stream readings to calibrate wake_mm / sleep_mm
      _watchUntil = millis() + (n >= 3 && isNum(t[2]) ? t[2].toInt() * 1000UL : 30000UL);
      _watchNext = 0;
      out.println("watching the sensor (Serial only), 'stop' ends it");
      return;
    }
    out.printf("sensor type: %s   %s   button GP%u: %s\r\n", Sensor.type(), Standalone.sensorActive() ? "PRESENT" : "nobody", PIN_BUTTON, Standalone.buttonDown() ? "down" : "up");
    if (Sensor.isDistance()) out.printf("distance: %ld mm   wake below %u mm, gone above %u mm, hold %u ms\r\n", (long)Sensor.distanceMm(), v.wakeMm, v.sleepMm, v.smoothMs);
    return;
  }
  if (cmd == "log") { out.print(_log.length() ? _log.c_str() : "(log empty)\r\n"); return; }

  if (cmd == "net") {
    Dashboard.printNet(out);
    Osc.printStatus(out);
    const uint32_t a = FurbyBus.ticks();
    delay(50);
    const uint32_t b = FurbyBus.ticks();
    out.printf("core 1: %s (%lu engine ticks in 50 ms, expect ~47)\r\n",
               !Dg.core1Enabled ? "PARKED by diag" : (b != a ? "running" : "NOT RUNNING"), (unsigned long)(b - a));
    out.printf("heap: %d bytes free of %d   uptime: %lu s\r\n", rp2040.getFreeHeap(), rp2040.getTotalHeap(), (unsigned long)(millis() / 1000));
    if (Dg.resetWatchdog && Dg.resetStage == CR_CLEAN_REBOOT) out.println("last reset: commanded reboot");
    else if (Dg.resetWatchdog) out.printf("last reset: WATCHDOG - code was in stage '%s'\r\n", crumbName(Dg.resetStage));
    else out.println("last reset: power-on / other (a brown-out would look like this)");
    diagPrintFlags(out);
    return;
  }
  if (cmd == "debug") {
    bool b;
    if (n < 2 || !onOff(t[1], b)) { out.printf("log is %s. usage: debug on|off\r\n", (Dg.flags & DIAG_QUIET) ? "off" : "on"); return; }
    if (b) Dg.flags &= ~DIAG_QUIET; else Dg.flags |= DIAG_QUIET;
    diagSave();
    out.printf("live log %s\r\n", b ? "on" : "off");
    return;
  }
  if (cmd == "diag") {
    if (n >= 2) {
      String sub = t[1]; sub.toLowerCase();
      if (sub == "reset") { Dg.flags &= DIAG_QUIET; Dg.channel = 0; }
      else if (sub == "channel") {
        if (n < 3 || !isNum(t[2]) || t[2].toInt() > 13) { out.println("usage: diag channel <0|1..13>"); return; }
        Dg.channel = t[2].toInt();
      } else if (sub == "off" || sub == "on") {
        for (int i = 2; i < n; i++) {
          String nm = t[i]; nm.toLowerCase();
          uint8_t bit;
          if (!diagFlagByName(nm, bit)) { out.printf("unknown subsystem '%s'\r\n", nm.c_str()); return; }
          if (sub == "off") Dg.flags |= bit; else Dg.flags &= ~bit;
        }
      } else { out.println("usage: diag | diag off|on <names> | diag channel <n> | diag reset"); return; }
      diagSave();
      out.println("saved - applies after 'reboot'");
    }
    diagPrintFlags(out);
    return;
  }

  // ── talking to furbies ──
  if (cmd == "wake") {
    uint16_t m; if (!needSet(1, m)) return;
    bool ok = true;
    forSet(m, [&](int i) { ok &= FurbyBus.wake(i); });
    out.print("wake "); printSet(out, m); out.println(ok ? "" : "  (queue full!)");
    return;
  }
  if (cmd == "sleep") {
    uint16_t m; if (!needSet(1, m)) return;
    bool ok = true;
    forSet(m, [&](int i) { ok &= FurbyBus.sleep(i); });
    out.print("sleep "); printSet(out, m); out.println(ok ? "" : "  (queue full!)");
    return;
  }
  if (cmd == "send") {
    uint16_t m; uint8_t code;
    if (!needSet(1, m)) return;
    if (n < 3 || !parseCode(t[2], code)) { out.println("need a code: a..p, 0..15, name, dance, sneeze, sheep, bed, sleep"); return; }
    uint8_t reps = (n >= 4 && isNum(t[3])) ? (uint8_t)constrain(t[3].toInt(), 1, 50) : 0;
    bool ok = true;
    forSet(m, [&](int i) { ok &= FurbyBus.send(i, code, reps); });
    out.printf("send %c(%u) to ", FurbyProtocol::letterFor(code), code); printSet(out, m); out.println(ok ? "" : "  (queue full!)");
    return;
  }
  if (cmd == "wakeall")  { Standalone.wakeAll();  out.println("waking everyone, staggered"); return; }
  if (cmd == "sleepall") { Standalone.sleepAll(); out.println("putting everyone to sleep, staggered"); return; }
  if (cmd == "trigger")  { Standalone.trigger();  out.println("triggered"); return; }

  // ── bench tests ──
  if (cmd == "pulse") {
    uint16_t m; if (!needSet(1, m)) return;
    if (n < 3 || !isNum(t[2])) { out.println("usage: pulse <set> <ms>"); return; }
    const uint16_t ms = constrain(t[2].toInt(), 1, 10000);
    forSet(m, [&](int i) { FurbyBus.level(i, true, ms); });
    out.printf("HIGH for %ums on ", ms); printSet(out, m); out.println(", then float");
    return;
  }
  if (cmd == "hold") {
    uint16_t m; if (!needSet(1, m)) return;
    String mode = n >= 3 ? t[2] : String();
    mode.toLowerCase();
    if (mode == "float") { forSet(m, [&](int i) { FurbyBus.clear(i); }); out.print("released "); printSet(out, m); out.println(); return; }
    if (mode != "high" && mode != "low") { out.println("usage: hold <set> high|low|float [ms]"); return; }
    const uint16_t ms = (n >= 4 && isNum(t[3])) ? (uint16_t)constrain(t[3].toInt(), 1, 60000) : 0xFFFF;
    forSet(m, [&](int i) { FurbyBus.level(i, mode == "high", ms); });
    out.printf("holding %s on ", mode.c_str()); printSet(out, m);
    if (ms == 0xFFFF) out.print(" until 'hold <set> float' or 'stop'");
    out.println();
    return;
  }
  if (cmd == "sweep") {
    if (n < 2 || !isNum(t[1]) || t[1].toInt() < 1 || t[1].toInt() > NUM_FURBIES) { out.println("usage: sweep <furby n> [from_us [to_us [step_us]]]"); return; }
    const int f = t[1].toInt();
    int from = n >= 3 ? t[2].toInt() : 20;
    int to   = n >= 4 ? t[3].toInt() : 200;
    int step = n >= 5 ? t[4].toInt() : 20;
    from = constrain(from, 5, 500); to = constrain(to, from, 500); step = max(step, 1);
    std::vector<Step> s;
    s.push_back({0, String("wake ") + f});
    for (int w = from; w <= to; w += step) {      // 4.5 s: lets the furby finish talking before the next try
      s.push_back({4500, String("_pulse ") + w});
      s.push_back({0, String("send ") + f + " name 3"});
    }
    s.push_back({4500, String("_pulse ") + v.pulseUs});
    s.push_back({0, String("sleep ") + f});
    out.printf("sweep on furby %d: pulse width %d..%d us step %d. Note which widths make it say its name.\r\n", f, from, to, step);
    startScript(s, out);
    return;
  }
  if (cmd == "_pulse") {            // internal: temporary pulse width for sweeps (not stored)
    if (n >= 2 && isNum(t[1])) { FurbyBus.setPulseOnly(t[1].toInt()); out.printf("pulse width now %s us (temporary)\r\n", t[1].c_str()); }
    return;
  }
  if (cmd == "selftest") {
    uint16_t m; if (!needSet(1, m)) return;
    String set = t[1];
    std::vector<Step> s;
    s.push_back({0,     String("wake ") + set});
    s.push_back({4500,  String("send ") + set + " name"});
    s.push_back({5000,  String("send ") + set + " dance"});
    s.push_back({8000,  String("sleep ") + set});
    startScript(s, out);
    return;
  }
  if (cmd == "stop") {
    stopScript();
    _watchUntil = 0;
    FurbyBus.clearAll();
    applyTiming();                  // undo any temporary pulse width
    out.println("stopped: scripts aborted, queues cleared, all wires released");
    return;
  }

  // ── configuration ──
  if (cmd == "osc" || cmd == "peers") {
    String sub = (cmd == "peers") ? String("status") : (n >= 2 ? t[1] : String("status"));
    sub.toLowerCase();
    if (sub == "status") { Osc.printStatus(out); return; }
    if (sub == "send") {                                   // osc send /furby/hub/2/wake 3,4   (test / debug)
      if (n < 3 || t[2][0] != '/') { out.println("usage: osc send /address [args...]   (ints, floats, text)"); return; }
      OscCodec::Arg a[OscCodec::MAX_ARGS];
      uint8_t na = 0;
      for (int i = 3; i < n && na < OscCodec::MAX_ARGS; i++) {
        char* end = nullptr;
        const long iv = strtol(t[i].c_str(), &end, 10);
        if (t[i].length() && !*end) a[na++] = OscCodec::argI((int32_t)iv);
        else { const float fv = strtof(t[i].c_str(), &end); a[na++] = (t[i].length() && !*end) ? OscCodec::argF(fv) : OscCodec::argS(t[i].c_str()); }
      }
      out.println(Osc.send(t[2].c_str(), a, na) ? "sent" : "not sent (osc inactive or message too long)");
      return;
    }
    out.println("usage: osc status | osc send /address [args...] | peers");
    return;
  }
  if (cmd == "config") {
    String sub = n >= 2 ? t[1] : String("show");
    sub.toLowerCase();
    if (sub == "show") { out.println(Cfg.json(true, true)); return; }
    if (sub == "get") {
      String r;
      if (n < 3) { out.println("usage: config get <path>"); return; }
      if (Cfg.getPath(t[2], r)) out.println(r); else out.printf("unknown key '%s' (try: config show)\r\n", t[2].c_str());
      return;
    }
    if (sub == "set") {
      if (n < 4) { out.println("usage: config set <path> <value>"); return; }
      const String value = restOf(line, 3);
      String err; bool reboot = false;
      if (Cfg.setPath(t[2], value, err, reboot)) {
        applyTiming();
        out.println("ok (RAM only - 'save' to keep it)");
        if (reboot) out.println("this setting needs a reboot to take effect (after 'save')");
      } else out.println(err);
      return;
    }
    if (sub == "reload") { Cfg.load(); applyTiming(); out.println(Cfg.loadNote); return; }
    out.println("usage: config show | get <path> | set <path> <value> | reload");
    return;
  }
  if (cmd == "rule") {
    String sub = n >= 2 ? t[1] : String("list");
    sub.toLowerCase();
    if (sub == "list") {
      if (!v.ruleCount) out.println("no rules");
      for (int i = 0; i < v.ruleCount; i++) out.printf("%d: on %s  after %ums  do: %s\r\n", i, v.rules[i].on, v.rules[i].delayMs, v.rules[i].act);
      return;
    }
    if (sub == "clear") { v.ruleCount = 0; out.println("rules cleared (RAM only - 'save' to keep)"); return; }
    if (sub == "del") {
      if (n < 3 || !isNum(t[2]) || t[2].toInt() >= v.ruleCount) { out.println("usage: rule del <n>  (see 'rule list')"); return; }
      for (int i = t[2].toInt(); i + 1 < v.ruleCount; i++) v.rules[i] = v.rules[i + 1];
      v.ruleCount--;
      out.println("deleted (RAM only - 'save' to keep)");
      return;
    }
    if (sub == "add") {
      const String act = restOf(line, 4);
      if (n < 5 || !isNum(t[3]) || !act.length()) { out.println("usage: rule add <hub/N/what[=value]> <delay_ms> <command...>"); return; }
      if (v.ruleCount >= MAX_RULES) { out.println("at most 8 rules"); return; }
      Rule& r = v.rules[v.ruleCount];
      memset(&r, 0, sizeof r);
      strlcpy(r.on, t[2].c_str(), sizeof r.on);
      strlcpy(r.act, act.c_str(), sizeof r.act);
      r.delayMs = (uint16_t)constrain(t[3].toInt(), 0, 60000);
      v.ruleCount++;
      out.printf("rule %d added (RAM only - 'save' to keep)\r\n", v.ruleCount - 1);
      return;
    }
    out.println("usage: rule list | add <on> <delay_ms> <command...> | del <n> | clear");
    return;
  }
  if (cmd == "enable" || cmd == "disable") {
    uint16_t m; if (!needSet(1, m)) return;
    // 'all' for enable means every furby, not just the currently enabled ones
    if (cmd == "enable" && t[1] == "all") m = 0xFFFF;
    if (cmd == "enable") v.enabledMask |= m; else v.enabledMask &= ~m;
    Cfg.save();
    out.print("enabled: "); printSet(out, v.enabledMask); out.println();
    return;
  }
  if (cmd == "auto") {
    bool b;
    if (n < 2 || !onOff(t[1], b)) { out.printf("auto is %s. usage: auto on|off\r\n", v.autoEnabled ? "on" : "off"); return; }
    v.autoEnabled = b;
    Cfg.save();
    out.printf("auto %s\r\n", b ? "on: sensor, timeout and chatter active" : "off: manual only");
    return;
  }
  if (cmd == "timing") {
    if (n >= 3 && isNum(t[2])) {
      String k = t[1]; k.toLowerCase();
      const long val = t[2].toInt();
      if      (k == "pulse")   v.pulseUs   = constrain(val, 5, 500);
      else if (k == "half")    v.halfBitUs = constrain(val, 600, 3000);
      else if (k == "bit")     v.halfBitUs = constrain(val / 2, 600, 3000);
      else if (k == "repeats") v.repeats   = constrain(val, 1, 50);
      else if (k == "gap")     v.gapMs     = constrain(val, 0, 1000);
      else if (k == "wake")    v.wakeMs    = constrain(val, 5, 5000);
      else { out.println("timing keys: pulse bit half repeats gap wake"); return; }
      applyTiming();
      out.println("applied (RAM only - 'save' to keep)");
    }
    out.printf("pulse=%uus  half-bit=%uus (bit=%uus)  repeats=%u  gap=%ums  wake=%ums\r\n",
               v.pulseUs, v.halfBitUs, v.halfBitUs * 2, v.repeats, v.gapMs, v.wakeMs);
    return;
  }
  if (cmd == "set") {
    if (n < 3) { out.println("usage: set <awake|chatmin|chatmax|cooldown|wakestagger|sleepstagger|chatter> <value>"); return; }
    String k = t[1]; k.toLowerCase();
    if (k == "chatter") {
      bool b; if (!onOff(t[2], b)) { out.println("usage: set chatter on|off"); return; }
      v.chatter = b;
    } else if (isNum(t[2])) {
      const long val = t[2].toInt();
      if      (k == "awake")        v.awakeMs        = constrain(val, 1, 3600) * 1000UL;
      else if (k == "chatmin")      v.chatMinMs      = constrain(val, 1, 600) * 1000UL;
      else if (k == "chatmax")      v.chatMaxMs      = constrain(val, 1, 600) * 1000UL;
      else if (k == "cooldown")     v.cooldownMs     = constrain(val, 0, 600) * 1000UL;
      else if (k == "wakestagger")  v.wakeStaggerMs  = constrain(val, 0, 10000);
      else if (k == "sleepstagger") v.sleepStaggerMs = constrain(val, 0, 10000);
      else { out.println("unknown setting"); return; }
    } else { out.println("value must be a number"); return; }
    out.println("set (RAM only - 'save' to keep)");
    return;
  }
  if (cmd == "unit") {
    if (n < 2 || !isNum(t[1]) || t[1].toInt() < 1 || t[1].toInt() > 99) { out.printf("unit is %u. usage: unit <1..99>\r\n", v.unitId); return; }
    v.unitId = t[1].toInt();
    Cfg.save();
    out.printf("unit %u saved - SSID becomes %s%u after 'reboot'\r\n", v.unitId, WIFI_SSID_PREFIX, v.unitId);
    return;
  }
  if (cmd == "save")     { out.println(Cfg.save() ? "saved to furbyhub.json" : (Cfg.fsOk() ? "save FAILED" : "save FAILED: LittleFS is not mounted")); return; }
  if (cmd == "defaults") { const uint8_t u = v.unitId; Cfg.defaults(); v.unitId = u; applyTiming(); Cfg.save(); out.println("defaults restored (unit number kept)"); return; }
  if (cmd == "reboot")   { out.println("rebooting..."); crumb(CR_CLEAN_REBOOT); delay(100); rp2040.reboot(); return; }

  out.printf("unknown command '%s' - try 'help'\r\n", cmd.c_str());
}
