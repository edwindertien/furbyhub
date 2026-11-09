#include "Settings.h"
#include <LittleFS.h>
#include <EEPROM.h>
#include "FurbyBus.h"

SettingsStore Cfg;

static const char* CFG_PATH = "/furbyhub.json";
static const char* CFG_TMP  = "/furbyhub.tmp";
static const char* REDACTED = "********";
static constexpr size_t CFG_MAX_BYTES = 8192;

void eepromBegin() {
  static bool done = false;
  if (!done) { EEPROM.begin(256); done = true; }
}

template <size_t N> static void cps(char (&dst)[N], const char* s) { strlcpy(dst, s ? s : "", N); }

// ───────────────────────── defaults ─────────────────────────

void SettingsStore::defaults() {
  memset(&v, 0, sizeof(v));
  v.unitId = 1;
  cps(v.hubName, "Hub 1");
  v.autoEnabled = 1;
  v.enabledMask = 0xFFFF;

  cps(v.netMode, "ap");
  cps(v.apPass, "furbyfurby");
  cps(v.mask, "255.255.255.0");
  v.oscPort = 9000;

  v.pulseUs = 150;  v.halfBitUs = 1050;  v.gapMs = 90;  v.wakeMs = 100;  v.repeats = 6;

  v.chatter = 1;
  v.awakeMs = 60000;                 // the original sketch kept furbies awake for 60 s
  v.wakeStaggerMs = 300;  v.sleepStaggerMs = 150;
  v.chatMinMs = 6000;     v.chatMaxMs = 20000;   v.cooldownMs = 3000;

  cps(v.sensorType, "pir");
  v.wakeMm = 1200;  v.sleepMm = 1500;  v.smoothMs = 200;

  for (auto& f : v.furby) cps(f.eyes, "blue");
}

void SettingsStore::resetNetwork() {
  cps(v.netMode, "ap");
  v.staSsid[0] = v.staPass[0] = v.ip[0] = v.gateway[0] = v.oscTarget[0] = 0;
  cps(v.apPass, "furbyfurby");
  cps(v.mask, "255.255.255.0");
}

// ───────────────────────── struct <-> JSON ─────────────────────────

void SettingsStore::toDoc(JsonDocument& d, bool redact) const {
  // ArduinoJson keeps a plain const char* by POINTER. fromDoc() later rewrites our own buffers while
  // reading this document (config set ...), so every string must be COPIED into the document.
  auto cp = [](const char* s) { return JsonString(s, JsonString::Copied); };
  auto secret = [&](const char* s) { return cp((redact && s[0]) ? REDACTED : s); };

  d["version"] = 1;

  JsonObject hub = d["hub"].to<JsonObject>();
  hub["id"] = v.unitId;
  hub["name"] = cp(v.hubName);
  hub["auto"] = (bool)v.autoEnabled;

  JsonObject n = d["network"].to<JsonObject>();
  n["mode"] = cp(v.netMode);
  n["ssid"] = cp(v.staSsid);
  n["pass"] = secret(v.staPass);
  n["ap_pass"] = secret(v.apPass);
  n["ip"] = cp(v.ip);
  n["gateway"] = cp(v.gateway);
  n["mask"] = cp(v.mask);
  n["osc_port"] = v.oscPort;
  n["osc_target"] = cp(v.oscTarget);

  JsonObject p = d["protocol"].to<JsonObject>();
  p["pulse_us"] = v.pulseUs;
  p["half_bit_us"] = v.halfBitUs;
  p["repeats"] = v.repeats;
  p["gap_ms"] = v.gapMs;
  p["wake_ms"] = v.wakeMs;

  JsonObject s = d["standalone"].to<JsonObject>();
  s["awake_s"] = v.awakeMs / 1000;
  s["wake_stagger_ms"] = v.wakeStaggerMs;
  s["sleep_stagger_ms"] = v.sleepStaggerMs;
  s["chatter"] = (bool)v.chatter;
  s["chat_min_s"] = v.chatMinMs / 1000;
  s["chat_max_s"] = v.chatMaxMs / 1000;
  s["cooldown_s"] = v.cooldownMs / 1000;

  JsonObject se = d["sensor"].to<JsonObject>();
  se["type"] = cp(v.sensorType);
  se["wake_mm"] = v.wakeMm;
  se["sleep_mm"] = v.sleepMm;
  se["smooth_ms"] = v.smoothMs;

  JsonObject fo = d["furbies"].to<JsonObject>();
  for (int i = 0; i < NUM_FURBIES; i++) {
    JsonObject f = fo[String(i + 1)].to<JsonObject>();
    f["enabled"] = (bool)(v.enabledMask & (1u << i));
    f["name"] = cp(v.furby[i].name);
    f["preset"] = cp(v.furby[i].preset);
    f["eyes"] = cp(v.furby[i].eyes);
  }

  JsonArray r = d["rules"].to<JsonArray>();
  for (int i = 0; i < v.ruleCount; i++) {
    JsonObject o = r.add<JsonObject>();
    o["on"] = cp(v.rules[i].on);
    o["do"] = cp(v.rules[i].act);
    o["delay_ms"] = v.rules[i].delayMs;
  }
}

namespace {

void problem(String& p, const String& s) { if (p.length()) p += "; "; p += s; }

template <typename T>
void num(JsonVariantConst o, const char* key, T& dst, long lo, long hi, long scale = 1) {
  JsonVariantConst x = o[key];
  if (x.isNull()) return;
  if (!x.is<long>()) return;
  dst = (T)(constrain(x.as<long>(), lo, hi) * scale);
}

void flag(JsonVariantConst o, const char* key, uint8_t& dst) {
  JsonVariantConst x = o[key];
  if (x.is<bool>()) dst = x.as<bool>() ? 1 : 0;
}

template <size_t N>
void str(JsonVariantConst o, const char* key, char (&dst)[N], bool secret = false) {
  JsonVariantConst x = o[key];
  if (!x.is<const char*>()) return;
  const char* s = x.as<const char*>();
  if (secret && strcmp(s, REDACTED) == 0) return;      // untouched placeholder: keep the stored value
  strlcpy(dst, s, N);
}

bool oneOf(const char* s, std::initializer_list<const char*> opts) {
  for (auto o : opts) if (strcmp(s, o) == 0) return true;
  return false;
}

}  // namespace

void SettingsStore::fromDoc(JsonVariantConst d, String& prob) {
  JsonVariantConst hub = d["hub"];
  num(hub, "id", v.unitId, 1, 99);
  str(hub, "name", v.hubName);
  flag(hub, "auto", v.autoEnabled);

  JsonVariantConst n = d["network"];
  str(n, "mode", v.netMode);
  str(n, "ssid", v.staSsid);
  str(n, "pass", v.staPass, true);
  str(n, "ap_pass", v.apPass, true);
  str(n, "ip", v.ip);
  str(n, "gateway", v.gateway);
  str(n, "mask", v.mask);
  str(n, "osc_target", v.oscTarget);
  num(n, "osc_port", v.oscPort, 1024, 65535);

  JsonVariantConst p = d["protocol"];
  num(p, "pulse_us", v.pulseUs, 5, 500);
  num(p, "half_bit_us", v.halfBitUs, 600, 3000);
  num(p, "repeats", v.repeats, 1, 50);
  num(p, "gap_ms", v.gapMs, 0, 1000);
  num(p, "wake_ms", v.wakeMs, 5, 5000);

  JsonVariantConst s = d["standalone"];
  num(s, "awake_s", v.awakeMs, 1, 3600, 1000);
  num(s, "wake_stagger_ms", v.wakeStaggerMs, 0, 10000);
  num(s, "sleep_stagger_ms", v.sleepStaggerMs, 0, 10000);
  flag(s, "chatter", v.chatter);
  num(s, "chat_min_s", v.chatMinMs, 1, 600, 1000);
  num(s, "chat_max_s", v.chatMaxMs, 1, 600, 1000);
  num(s, "cooldown_s", v.cooldownMs, 0, 600, 1000);

  JsonVariantConst se = d["sensor"];
  str(se, "type", v.sensorType);
  num(se, "wake_mm", v.wakeMm, 20, 4000);
  num(se, "sleep_mm", v.sleepMm, 20, 4000);
  num(se, "smooth_ms", v.smoothMs, 0, 5000);

  JsonVariantConst fo = d["furbies"];
  if (fo.is<JsonObjectConst>()) {
    for (int i = 0; i < NUM_FURBIES; i++) {
      JsonVariantConst f = fo[String(i + 1)];
      if (f.isNull()) continue;
      JsonVariantConst en = f["enabled"];
      if (en.is<bool>()) { if (en.as<bool>()) v.enabledMask |= (1u << i); else v.enabledMask &= ~(1u << i); }
      str(f, "name", v.furby[i].name);
      str(f, "preset", v.furby[i].preset);
      str(f, "eyes", v.furby[i].eyes);
    }
  }

  JsonVariantConst rl = d["rules"];
  if (rl.is<JsonArrayConst>()) {
    uint8_t c = 0;
    for (JsonVariantConst o : rl.as<JsonArrayConst>()) {
      if (c >= MAX_RULES) { problem(prob, "at most 8 rules"); break; }
      Rule& r = v.rules[c];
      memset(&r, 0, sizeof r);
      str(o, "on", r.on);
      str(o, "do", r.act);
      num(o, "delay_ms", r.delayMs, 0, 60000);
      if (!r.on[0] || !r.act[0]) { problem(prob, "rule " + String(c) + " needs both 'on' and 'do'"); continue; }
      c++;
    }
    v.ruleCount = c;
  }

  // ── validation ──
  if (!oneOf(v.netMode, {"ap", "sta"}))              problem(prob, "network.mode must be ap or sta");
  if (!oneOf(v.sensorType, {"none", "pir", "hcsr04"})) problem(prob, "sensor.type must be none, pir or hcsr04");
  if (v.apPass[0] && strlen(v.apPass) < 8)           problem(prob, "network.ap_pass needs at least 8 characters (WPA2)");
  if (v.staPass[0] && strlen(v.staPass) < 8)         problem(prob, "network.pass needs at least 8 characters (WPA2)");
  for (auto* a : {v.ip, v.gateway, v.mask, v.oscTarget}) {
    IPAddress t;
    if (a[0] && !t.fromString(a))                    problem(prob, String("not an IP address: ") + a);
  }
  if (!strcmp(v.netMode, "sta") && !v.staSsid[0])    problem(prob, "network.ssid is required in sta mode");
  if (v.sleepMm < v.wakeMm) v.sleepMm = v.wakeMm;
  if (v.chatMaxMs <= v.chatMinMs) v.chatMaxMs = v.chatMinMs + 1000;
}

bool SettingsStore::applyDoc(JsonVariantConst d, String& err, bool& needsReboot) {
  const Settings old = v;
  String prob;
  fromDoc(d, prob);
  if (prob.length()) { v = old; err = prob; return false; }
  needsReboot = old.unitId != v.unitId || strcmp(old.netMode, v.netMode) || strcmp(old.staSsid, v.staSsid) ||
                strcmp(old.staPass, v.staPass) || strcmp(old.apPass, v.apPass) || strcmp(old.ip, v.ip) ||
                strcmp(old.gateway, v.gateway) || strcmp(old.mask, v.mask) || old.oscPort != v.oscPort ||
                strcmp(old.oscTarget, v.oscTarget) || strcmp(old.sensorType, v.sensorType);
  return true;
}

String SettingsStore::json(bool redact, bool pretty) const {
  JsonDocument d;
  toDoc(d, redact);
  String out;
  if (pretty) serializeJsonPretty(d, out); else serializeJson(d, out);
  return out;
}

bool SettingsStore::applyJson(const String& text, String& err, bool& needsReboot) {
  needsReboot = false;
  JsonDocument d;
  const DeserializationError e = deserializeJson(d, text);
  if (e) { err = String("JSON error: ") + e.c_str(); return false; }
  if (!d.is<JsonObject>()) { err = "top level must be a JSON object"; return false; }
  return applyDoc(d, err, needsReboot);
}

// ── dotted paths: network.ssid, furbies.3.preset, rules.0.do ──

static JsonVariant walk(JsonVariant cur, const String& path) {
  int start = 0;
  while (start <= (int)path.length() && !cur.isNull()) {
    int dot = path.indexOf('.', start);
    if (dot < 0) dot = path.length();
    const String seg = path.substring(start, dot);
    if (cur.is<JsonArray>()) {
      bool digits = seg.length() > 0;
      for (unsigned i = 0; i < seg.length(); i++) if (!isDigit(seg[i])) digits = false;
      if (digits) cur = cur[(size_t)seg.toInt()].as<JsonVariant>();
      else        cur = JsonVariant();
    } else if (cur.is<JsonObject>()) {
      cur = cur[seg].as<JsonVariant>();
    } else {
      return JsonVariant();
    }
    start = dot + 1;
  }
  return cur;
}

bool SettingsStore::getPath(const String& path, String& out) const {
  JsonDocument d;
  toDoc(d, true);
  JsonVariant x = path.length() ? walk(d.as<JsonVariant>(), path) : d.as<JsonVariant>();
  if (x.isNull()) return false;
  if (x.is<const char*>()) out = x.as<const char*>();
  else serializeJsonPretty(x, out);
  return true;
}

bool SettingsStore::setPath(const String& path, const String& value, String& err, bool& needsReboot) {
  needsReboot = false;
  JsonDocument d;
  toDoc(d, false);
  JsonVariant x = walk(d.as<JsonVariant>(), path);
  if (x.isNull()) { err = "unknown key '" + path + "' (try: config show)"; return false; }
  if (x.is<JsonObject>() || x.is<JsonArray>()) { err = "'" + path + "' is a section, set one of its values"; return false; }

  String low = value; low.toLowerCase();
  if (x.is<bool>()) {
    if (low == "true" || low == "on" || low == "1" || low == "yes")       x.set(true);
    else if (low == "false" || low == "off" || low == "0" || low == "no") x.set(false);
    else { err = "'" + path + "' is true/false"; return false; }
  } else if (x.is<long>() || x.is<double>()) {
    char* end = nullptr;
    const long n = strtol(value.c_str(), &end, 10);
    if (!value.length() || *end) { err = "'" + path + "' needs a number"; return false; }
    x.set(n);
  } else {
    x.set(value);                               // copies the String
  }
  return applyDoc(d, err, needsReboot);
}

// ───────────────────────── files ─────────────────────────

bool SettingsStore::save() {
  if (!_fsOk) return false;
  const String s = json(false, true);
  File f = LittleFS.open(CFG_TMP, "w");
  if (!f) return false;
  const size_t w = f.print(s);
  f.close();
  File chk = LittleFS.open(CFG_TMP, "r");
  const bool good = (w == s.length()) && chk && chk.size() == s.length();
  if (chk) chk.close();
  if (!good) { LittleFS.remove(CFG_TMP); return false; }
  if (!LittleFS.rename(CFG_TMP, CFG_PATH)) {     // rename normally replaces atomically; older cores need the remove
    LittleFS.remove(CFG_PATH);
    return LittleFS.rename(CFG_TMP, CFG_PATH);
  }
  return true;
}

namespace {
struct LegacyV1 {                                // the old EEPROM layout, only for the one-time migration
  uint32_t magic; uint16_t version; uint8_t unitId; uint8_t autoEnabled; uint16_t enabledMask;
  uint16_t pulseUs; uint16_t halfBitUs; uint16_t gapMs; uint16_t wakeMs; uint8_t repeats; uint8_t chatter;
  uint32_t awakeMs; uint16_t wakeStaggerMs; uint16_t sleepStaggerMs; uint16_t chatMinMs; uint16_t chatMaxMs;
  uint16_t cooldownMs; uint16_t crc;
};
uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) { crc ^= (uint16_t)(*p++) << 8; for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1); }
  return crc;
}
}  // namespace

void SettingsStore::load() {
  defaults();
  _fsOk = LittleFS.begin();
  if (!_fsOk) { loadNote = "LittleFS mount FAILED: running on defaults, nothing can be saved"; return; }

  String lastErr;
  for (const char* path : {CFG_PATH, CFG_TMP}) {        // the temp file only matters after a power cut during save
    if (!LittleFS.exists(path)) continue;
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    if (f.size() > CFG_MAX_BYTES) { lastErr = String(path) + " is too large"; f.close(); continue; }
    const String text = f.readString();
    f.close();
    String err; bool reboot;
    if (applyJson(text, err, reboot)) { loadNote = (path == CFG_PATH) ? "loaded furbyhub.json" : "recovered from furbyhub.tmp"; return; }
    defaults();
    lastErr = String(path) + ": " + err;
  }
  if (lastErr.length()) { loadNote = "furbyhub.json INVALID (" + lastErr + "): running on defaults. Fix it or 'save' to overwrite"; return; }

  // No file yet: take over the settings of the earlier firmware once, then write the file.
  eepromBegin();
  LegacyV1 old;
  EEPROM.get(0, old);
  if (old.magic == 0x46555242 && old.version == 1 && old.crc == crc16((const uint8_t*)&old, sizeof(old) - 2)) {
    v.unitId = old.unitId;       v.autoEnabled = old.autoEnabled;  v.enabledMask = old.enabledMask;
    v.pulseUs = old.pulseUs;     v.halfBitUs = old.halfBitUs;      v.gapMs = old.gapMs;
    v.wakeMs = old.wakeMs;       v.repeats = old.repeats;          v.chatter = old.chatter;
    v.awakeMs = old.awakeMs;     v.wakeStaggerMs = old.wakeStaggerMs;  v.sleepStaggerMs = old.sleepStaggerMs;
    v.chatMinMs = old.chatMinMs; v.chatMaxMs = old.chatMaxMs;      v.cooldownMs = old.cooldownMs;
    snprintf(v.hubName, sizeof v.hubName, "Hub %u", v.unitId);
    loadNote = save() ? "migrated the old flash settings into furbyhub.json" : "migrated old settings but could not write furbyhub.json";
  } else {
    loadNote = save() ? "no config found: wrote defaults to furbyhub.json" : "no config found: defaults (could not write file)";
  }
}

void SettingsStore::applyRuntime() {
  BusTiming t{v.pulseUs, v.halfBitUs, v.gapMs, v.wakeMs, v.repeats};
  FurbyBus.setTiming(t);
}
