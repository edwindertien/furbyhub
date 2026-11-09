#include "Osc.h"
#include "Cli.h"
#include "Dashboard.h"
#include "Diag.h"
#include "RuleMatch.h"
#include "Sensor.h"
#include "Settings.h"
#include "Standalone.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <pico/cyw43_arch.h>

OscClass Osc;

namespace {
WiFiUDP udp;
using namespace OscCodec;

class NullPrint : public Print {
public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t n) override { return n; }
};

inline bool due(uint32_t t) { return (int32_t)(millis() - t) >= 0; }
}  // namespace

void OscClass::begin() { _nextBeat = millis() + 2000; }

void OscClass::startUdp() {
  IPAddress target;
  if (Cfg.v.oscTarget[0] && target.fromString(Cfg.v.oscTarget)) {
    // explicit target (e.g. a laptop running Max / TouchDesigner)
  } else {
    cyw43_arch_lwip_begin();
    const IPAddress ip = WiFi.localIP(), mask = WiFi.subnetMask();
    cyw43_arch_lwip_end();
    for (int i = 0; i < 4; i++) target[i] = ip[i] | (uint8_t)~mask[i];     // subnet broadcast
  }
  _target = (uint32_t)target;
  udp.stop();
  _active = udp.begin(Cfg.v.oscPort);
  _firstSend = true;
  dlog("OSC: %s, listening on UDP %u, sending to %s", _active ? "started" : "FAILED to start", Cfg.v.oscPort, target.toString().c_str());
}

// ── sending ─────────────────────────────────────────────────────────────────

bool OscClass::send(const char* address, const Arg* args, uint8_t n) {
  if (!_active) return false;
  uint8_t buf[160];
  const size_t len = encode(buf, sizeof buf, address, args, n);
  if (!len) return false;
  udp.beginPacket(IPAddress(_target), Cfg.v.oscPort);
  udp.write(buf, len);
  const bool ok = udp.endPacket();
  _sent++;
  return ok;
}

void OscClass::sendEvent(const char* what, int32_t v) {
  char addr[40];
  snprintf(addr, sizeof addr, "/furby/hub/%u/%s", Cfg.v.unitId, what);
  const Arg a = argI(v);
  send(addr, &a, 1);
}

// ── peers and rules ─────────────────────────────────────────────────────────

PeerInfo* OscClass::peerSlot(uint8_t id) {
  PeerInfo* oldest = &_peers[0];
  for (auto& p : _peers) {
    if (p.used && p.id == id) return &p;
    if (!p.used) { oldest = &p; break; }
    if ((int32_t)(oldest->lastMs - p.lastMs) > 0) oldest = &p;
  }
  *oldest = PeerInfo();
  oldest->used = true;
  oldest->id = id;
  return oldest;
}

void OscClass::fireRules(uint8_t id, const char* what, int32_t prev, int32_t cur) {
  for (uint8_t i = 0; i < Cfg.v.ruleCount && i < 8; i++) {
    const Rule& r = Cfg.v.rules[i];
    if (!ruleMatches(r.on, id, what, prev, cur)) continue;
    if (!due(_lastFire[i] + 300)) continue;                     // a rule fires at most every 300 ms
    _lastFire[i] = millis();
    _ruleHits++;
    for (auto& p : _pending) {
      if (p.used) continue;
      p.used = true; p.due = millis() + r.delayMs;
      strlcpy(p.line, r.act, sizeof p.line);
      dlog("rule %u fired by hub %u %s=%ld -> '%s' in %u ms", i, id, what, (long)cur, r.act, r.delayMs);
      break;
    }
  }
}

void OscClass::peerEvent(uint8_t id, uint32_t ip, const char* what, int32_t value) {
  PeerInfo* p = peerSlot(id);
  p->lastMs = millis();
  p->ip = ip;
  if (!strcmp(what, "presence")) {
    const int32_t prev = p->presence;
    p->presence = value ? 1 : 0;
    if (prev != p->presence) fireRules(id, "presence", prev, p->presence);
  } else if (!strcmp(what, "distance")) {
    const int32_t prev = p->dist;
    p->dist = value;
    if (prev != value) fireRules(id, "distance", prev, value);
  }
}

void OscClass::runPending() {
  for (auto& p : _pending) {
    if (!p.used || !due(p.due)) continue;
    p.used = false;
    NullPrint np;
    Cli.execute(p.line, np);
  }
}

// ── receiving ───────────────────────────────────────────────────────────────

void OscClass::runCommand(const char* cmd, const Msg& m) {
  static const char* const allowed[] = {"wake", "sleep", "send", "wakeall", "sleepall", "trigger", "stop"};
  bool ok = false;
  for (auto a : allowed) if (!strcmp(cmd, a)) ok = true;
  if (!ok) return;
  String line = cmd;
  for (uint8_t k = 0; k < m.n; k++) {
    line += ' ';
    if (m.a[k].type == 's') line += m.a[k].s; else line += String(m.a[k].i);
  }
  dlog("OSC command: %s", line.c_str());
  NullPrint np;
  Cli.execute(line, np);
}

void OscClass::handlePacket(const uint8_t* b, size_t n, uint32_t fromIp) {
  Msg m;
  if (!decode(b, n, m)) { _bad++; return; }
  if (strncmp(m.addr, "/furby/hub/", 11) != 0) return;
  const char* p = m.addr + 11;
  char who[8]; size_t k = 0;
  while (*p && *p != '/' && k < sizeof who - 1) who[k++] = *p++;
  who[k] = 0;
  if (*p != '/') return;
  const char* cmd = p + 1;
  _received++;

  const bool all = !strcmp(who, "all");
  int id = 0; bool digits = k > 0;
  for (size_t i = 0; i < k; i++) { if (who[i] < '0' || who[i] > '9') digits = false; else id = id * 10 + (who[i] - '0'); }
  if (!all && !digits) return;

  if (!all && id != Cfg.v.unitId) {                          // news from another hub
    if (id < 1 || id > 99) return;
    if (!strcmp(cmd, "presence") && m.n >= 1)      peerEvent(id, fromIp, "presence", m.a[0].i);
    else if (!strcmp(cmd, "distance") && m.n >= 1) peerEvent(id, fromIp, "distance", m.a[0].i);
    else if (!strcmp(cmd, "status") && m.n >= 4) {
      peerEvent(id, fromIp, "presence", m.a[0].i);
      PeerInfo* pi = peerSlot(id);
      pi->dist = m.a[1].i; pi->state = (uint8_t)m.a[2].i; pi->uptime = (uint32_t)m.a[3].i;
    }
    return;
  }
  if (!all && id == Cfg.v.unitId && (!strcmp(cmd, "presence") || !strcmp(cmd, "status") || !strcmp(cmd, "distance"))) return;
  runCommand(cmd, m);                                        // addressed to us or to everybody
}

void OscClass::receive() {
  for (int i = 0; i < 8; i++) {
    const int sz = udp.parsePacket();
    if (sz <= 0) return;
    uint8_t buf[256];
    const int n = udp.read(buf, sizeof buf);
    if (n > 0 && sz <= (int)sizeof buf) handlePacket(buf, (size_t)n, (uint32_t)udp.remoteIP());
    else _bad++;
  }
}

// ── main loop ───────────────────────────────────────────────────────────────

void OscClass::loop() {
  const bool want = Dashboard.linkUp() && (!strcmp(Dashboard.mode(), "sta") || Cfg.v.oscTarget[0]);
  if (!want) { _active = false; return; }
  if (!_active) { startUdp(); if (!_active) return; }

  receive();
  runPending();

  const uint32_t now = millis();
  const bool pres = Sensor.present();
  if (_firstSend || pres != _lastPres) { sendEvent("presence", pres ? 1 : 0); _lastPres = pres; }
  if (Sensor.isDistance()) {
    const int32_t d = Sensor.distanceMm();
    if (abs(d - _lastDist) >= 25 && due(_lastDistAt + 100)) { sendEvent("distance", d); _lastDist = d; _lastDistAt = now; }
  }
  if (_firstSend || due(_nextBeat)) {
    char addr[40];
    snprintf(addr, sizeof addr, "/furby/hub/%u/status", Cfg.v.unitId);
    const Arg a[4] = { argI(pres ? 1 : 0), argI(Sensor.distanceMm()), argI((int32_t)Standalone.state()), argI((int32_t)(now / 1000)) };
    send(addr, a, 4);
    _nextBeat = now + 2000;
  }
  _firstSend = false;
}

void OscClass::printStatus(Print& out) const {
  if (!_active) { out.println("osc: not active (needs network.mode = sta, or a network.osc_target)"); return; }
  out.printf("osc: active on UDP %u, sending to %s   sent %lu  received %lu  malformed %lu  rule hits %lu\r\n", Cfg.v.oscPort,
             IPAddress(_target).toString().c_str(), (unsigned long)_sent, (unsigned long)_received, (unsigned long)_bad, (unsigned long)_ruleHits);
  bool any = false;
  for (const auto& p : _peers) {
    if (!p.used) continue;
    any = true;
    out.printf("  hub %u  %s  presence %s  distance %ld mm  state %u  up %lus  last heard %lus ago\r\n", p.id, IPAddress(p.ip).toString().c_str(),
               p.presence < 0 ? "?" : (p.presence ? "YES" : "no"), (long)p.dist, p.state, (unsigned long)p.uptime,
               (unsigned long)(max<int32_t>(0, (int32_t)(millis() - p.lastMs)) / 1000));
  }
  if (!any) out.println("  no other hubs heard yet");
}
