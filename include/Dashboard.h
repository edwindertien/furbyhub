#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// WiFi access point + captive portal + web dashboard.
//
//   GET /              the dashboard (embedded in flash, no filesystem needed)
//   GET /api/status    JSON: unit, mode, per-furby state
//   GET /api/cli?line= run any CLI command, plain-text answer
//
// Every unit is its own network ("reFurby-<unit>"), same IP on all of them
// (192.168.42.1). A DNS server answers every lookup with that IP so phones pop
// up the "sign in to network" page by themselves.
//
// The dashboard is deliberately a thin client over the CLI: buttons just send
// CLI lines, so the web UI and the serial console cannot drift apart.
// ---------------------------------------------------------------------------
class DashboardClass {
public:
  void begin();
  void loop();
  uint8_t stations() const { return _stations; }   // cached, refreshed every few seconds
  bool    apUp() const { return _apUp; }          // network is up (own AP, or joined the router)
  bool    linkUp() const { return _apUp; }
  const char* mode() const { return _sta ? "sta" : "ap"; }
  IPAddress ip() const;
  void    printNet(Print& out) const;   // AP / HTTP counters for the `net` CLI command

private:
  bool startAP();
  void staBegin();
  void restartAP();

  void refreshStations();

  bool     _apUp = false;
  bool     _sta = false;
  uint32_t _nextLinkCheck = 0, _staNextTry = 0;
  uint8_t  _staFails = 0;
  volatile uint8_t _stations = 0;
  uint32_t _nextStationPoll = 0;
  uint32_t _lastRequestMs = 0;
  uint32_t _lastClientMs = 0;
  uint32_t _nextApRetryMs = 0;
};

extern DashboardClass Dashboard;
