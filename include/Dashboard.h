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
  uint8_t stations() const;     // WiFi clients currently associated
  bool    apUp() const { return _apUp; }

private:
  bool startAP();
  void restartAP();

  bool     _apUp = false;
  uint32_t _lastRequestMs = 0;
  uint32_t _lastClientMs = 0;
  uint32_t _nextApRetryMs = 0;
};

extern DashboardClass Dashboard;
