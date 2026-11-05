#include "Dashboard.h"
#include "DashboardPage.h"
#include "Cli.h"
#include "FurbyBus.h"
#include "Settings.h"
#include "Standalone.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

DashboardClass Dashboard;

namespace {

WebServer server(80);
DNSServer dns;
const IPAddress AP_IP(AP_IP_OCTETS);
const IPAddress AP_MASK(255, 255, 255, 0);

constexpr uint32_t APRESTART_IDLE_MS = 10UL * 60UL * 1000UL;   // nobody around for 10 min: refresh the AP

uint32_t g_lastRequest = 0;

void sendNoCache() {
  server.sendHeader("Cache-Control", "no-store");
}

void redirectToRoot() {
  server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
  sendNoCache();
  server.send(302, "text/plain", "");
}

void handleRoot() {
  g_lastRequest = millis();
  sendNoCache();
  server.send(200, "text/html; charset=utf-8", DASH_HTML);
}

void handleStatus() {
  g_lastRequest = millis();
  const Settings& v = Cfg.v;
  String j;
  j.reserve(900);
  j += "{\"unit\":";      j += v.unitId;
  j += ",\"ssid\":\"";    j += WIFI_SSID_PREFIX; j += v.unitId; j += "\"";
  j += ",\"fw\":\"";      j += FW_VERSION; j += "\"";
  j += ",\"auto\":";      j += v.autoEnabled ? "true" : "false";
  j += ",\"state\":\"";   j += Standalone.stateName(); j += "\"";
  j += ",\"remaining\":"; j += (unsigned long)(Standalone.remainingMs() / 1000);
  j += ",\"sensor\":";    j += Standalone.sensorActive() ? "true" : "false";
  j += ",\"stations\":";  j += Dashboard.stations();
  j += ",\"script\":";    j += Cli.scriptRunning() ? "true" : "false";
  j += ",\"mask\":";      j += v.enabledMask;
  j += ",\"f\":[";
  const uint32_t now = millis();
  for (int i = 0; i < NUM_FURBIES; i++) {
    const ChannelStatus& s = FurbyBus.status(i);
    if (i) j += ',';
    j += '['; j += s.assumed;
    j += ',';  j += (s.lastCode == 0xFF ? -1 : (int)s.lastCode);
    j += ',';  j += s.busy ? 1 : 0;
    j += ',';  j += s.lastMs ? (long)((now - s.lastMs) / 1000) : -1L;
    j += ']';
  }
  j += "]}";
  sendNoCache();
  server.send(200, "application/json; charset=utf-8", j);
}

void handleCli() {
  g_lastRequest = millis();
  StringPrint out;
  Cli.execute(server.arg("line"), out);
  sendNoCache();
  server.send(200, "text/plain; charset=utf-8", out.s);
}

// Everything else (including the OS captive-portal probes such as /generate_204,
// /hotspot-detect.html, /connecttest.txt, /ncsi.txt ...) lands on the dashboard.
void handleNotFound() {
  redirectToRoot();
}

}  // namespace

bool DashboardClass::startAP() {
  const String ssid = String(WIFI_SSID_PREFIX) + Cfg.v.unitId;
  static const uint8_t channels[3] = {1, 6, 11};
  const uint8_t ch = channels[(Cfg.v.unitId - 1) % 3];

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  const bool ok = WiFi.softAP(ssid.c_str(), WIFI_PASS, ch);
  if (!ok) return false;
  dns.start(53, "*", AP_IP);
  server.begin();
  return true;
}

void DashboardClass::restartAP() {
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  delay(100);
  _apUp = startAP();
}

void DashboardClass::begin() {
  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/cli", handleCli);
  server.onNotFound(handleNotFound);
  _apUp = startAP();
  _lastRequestMs = _lastClientMs = millis();
  _nextApRetryMs = millis() + 5000;
}

uint8_t DashboardClass::stations() const {
  return _apUp ? WiFi.softAPgetStationNum() : 0;
}

void DashboardClass::loop() {
  const uint32_t now = millis();

  if (!_apUp) {                       // AP failed to come up: keep trying, the unit still works standalone
    if ((int32_t)(now - _nextApRetryMs) >= 0) {
      restartAP();
      _nextApRetryMs = now + 5000;
    }
    return;
  }

  dns.processNextRequest();
  server.handleClient();

  if (g_lastRequest) _lastRequestMs = g_lastRequest;
  if (stations() > 0) _lastClientMs = now;
  // Unattended installation: if nobody has been connected for a long time, rebuild the AP
  // (the original single-user remote did the same as a cheap cure for a stuck CYW43 AP).
  if (now - max(_lastRequestMs, _lastClientMs) > APRESTART_IDLE_MS) {
    restartAP();
    _lastRequestMs = _lastClientMs = now;
  }
}
