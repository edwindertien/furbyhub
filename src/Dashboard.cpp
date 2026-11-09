#include "Dashboard.h"
#include "DashboardPage.h"
#include "Cli.h"
#include "FurbyBus.h"
#include "Settings.h"
#include "Standalone.h"
#include "Sensor.h"
#include "config.h"
#include "Diag.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <pico/cyw43_arch.h>
#include <LEAmDNS.h>
#include "Osc.h"

// ---------------------------------------------------------------------------
// Transport: raw WiFiServer, one request per connection, closed immediately.
// This is the same pattern as the (working) picoControl vacuum-robot remote.
// The WebServer library was tried first and dropped: it serves ONE connection at
// a time and then waits up to 5 s for the browser to close it, so phones that open
// several sockets at once (captive-portal probes, page, status polling) stalled
// and showed an empty page.
// ---------------------------------------------------------------------------

DashboardClass Dashboard;

namespace {

WiFiServer server(80);
DNSServer  dns;
bool       g_dnsUp = false;
IPAddress  g_apIP(AP_IP_OCTETS);
const IPAddress AP_MASK(255, 255, 255, 0);

constexpr uint32_t APRESTART_IDLE_MS = 10UL * 60UL * 1000UL;   // nobody around for 10 min: refresh the AP
constexpr uint16_t SEND_TIMEOUT_MS   = 1500;                   // per blocking write; keeps us far from the 8 s watchdog

uint32_t g_lastRequest = 0;

// ── response helpers ────────────────────────────────────────────────────────

size_t writeAll(WiFiClient& c, const char* p, size_t n) {
  // Do NOT test c.connected() here: a client that half-closes after sending its request
  // (FIN) reports "not connected" while it is still waiting for the reply. Give up only
  // when no byte could be written for 2 s (peer really gone).
  size_t sent = 0;
  uint32_t last = millis();
  while (sent < n) {
    size_t chunk = n - sent;
    if (chunk > 512) chunk = 512;
    const size_t k = c.write((const uint8_t*)p + sent, chunk);
    rp2040.wdt_reset();
    if (k) { sent += k; last = millis(); delay(0); }
    else   { if (millis() - last > 2000) break; delay(1); }
  }
  return sent;
}

void sendHead(WiFiClient& c, const char* code, const char* type, size_t len, const char* location = nullptr) {
  c.print("HTTP/1.1 "); c.println(code);
  c.print("Content-Type: "); c.println(type);
  c.print("Content-Length: "); c.println(len);
  if (location) { c.print("Location: "); c.println(location); }
  c.println("Cache-Control: no-store");
  c.println("Connection: close");
  c.println();
}

size_t sendText(WiFiClient& c, const char* code, const char* type, const String& body) {
  sendHead(c, code, type, body.length());
  return writeAll(c, body.c_str(), body.length());
}

void redirectToRoot(WiFiClient& c) {
  const String loc = String("http://") + g_apIP.toString() + "/";
  sendHead(c, "302 Found", "text/plain", 0, loc.c_str());
}

// ── query parsing (same approach as the vacuum-robot remote) ────────────────

String urlDecode(const String& s) {
  String out; out.reserve(s.length());
  auto hex = [](char x) -> int {
    if (x >= '0' && x <= '9') return x - '0';
    if (x >= 'a' && x <= 'f') return 10 + (x - 'a');
    if (x >= 'A' && x <= 'F') return 10 + (x - 'A');
    return 0;
  };
  for (size_t i = 0; i < s.length(); i++) {
    const char ch = s[i];
    if (ch == '+') { out += ' '; continue; }
    if (ch == '%' && i + 2 < s.length()) {
      out += (char)((hex(s[i + 1]) << 4) | hex(s[i + 2]));
      i += 2;
      continue;
    }
    out += ch;
  }
  return out;
}

bool queryParam(const String& url, const char* key, String& value) {
  const int q = url.indexOf('?');
  if (q < 0) return false;
  const String qs = url.substring(q + 1);
  const String k = String(key) + "=";
  int start = 0;
  while (start < (int)qs.length()) {
    int amp = qs.indexOf('&', start);
    if (amp < 0) amp = qs.length();
    const String part = qs.substring(start, amp);
    if (part.startsWith(k)) { value = urlDecode(part.substring(k.length())); return true; }
    start = amp + 1;
  }
  return false;
}

// ── routes ──────────────────────────────────────────────────────────────────

String statusJson() {
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
  j += ",\"stype\":\"";   j += Sensor.type(); j += "\"";
  j += ",\"dist\":";      j += (long)Sensor.distanceMm();
  j += ",\"stations\":";  j += Dashboard.stations();
  j += ",\"script\":";    j += Cli.scriptRunning() ? "true" : "false";
  j += ",\"net\":\"";     j += Dashboard.mode(); j += "\"";
  j += ",\"want\":\"";    j += v.netMode; j += "\"";
  j += ",\"link\":";      j += Dashboard.linkUp() ? "true" : "false";
  j += ",\"mask\":";      j += v.enabledMask;
  j += ",\"f\":[";
  const uint32_t now = millis();
  for (int i = 0; i < NUM_FURBIES; i++) {
    const ChannelStatus& s = FurbyBus.status(i);
    if (i) j += ',';
    j += '['; j += s.assumed;
    j += ',';  j += (s.lastCode == 0xFF ? -1 : (int)s.lastCode);
    j += ',';  j += s.busy ? 1 : 0;
    j += ',';  j += s.lastMs ? (long)max<int32_t>(0, (int32_t)(now - s.lastMs)) / 1000 : -1L;
    j += ']';
  }
  j += "],\"peers\":[";
  bool firstPeer = true;
  for (uint8_t i = 0; i < MAX_PEERS; i++) {
    const PeerInfo& p = Osc.peer(i);
    if (!p.used) continue;
    if (!firstPeer) j += ',';
    firstPeer = false;
    j += '['; j += p.id; j += ','; j += (p.presence > 0 ? 1 : 0);
    j += ','; j += (long)p.dist;
    j += ','; j += (long)(max<int32_t>(0, (int32_t)(now - p.lastMs)) / 1000);
    j += ']';
  }
  j += "]}";
  return j;
}

// last request, for the `net` report
char     g_lastReq[72] = "-";
uint32_t g_lastReqAt = 0;

void handleClient() {
  crumb(CR_ACCEPT);
  WiFiClient client = server.accept();
  if (!client) return;

  const uint32_t t0 = millis();
  const String who = client.remoteIP().toString();

  crumb(CR_READ_REQ);
  client.setTimeout(150);
  String reqLine = client.readStringUntil('\n');
  reqLine.trim();

  crumb(CR_READ_HDR);
  size_t contentLength = 0;
  while (client.connected()) {                         // drain the headers (keep Content-Length for POST)
    const String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
    if (line.length() > 15 && (line[0] == 'C' || line[0] == 'c')) {
      String l = line; l.toLowerCase();
      if (l.startsWith("content-length:")) contentLength = (size_t)l.substring(15).toInt();
    }
  }

  const int sp1 = reqLine.indexOf(' ');
  const int sp2 = reqLine.indexOf(' ', sp1 + 1);
  if (sp1 < 0 || sp2 < 0) {                            // empty pre-connect or garbage
    Dg.httpEmpty++;
    dlog("http: %s opened a connection but sent no request (%lu ms)", who.c_str(), (unsigned long)(millis() - t0));
    client.stop();
    return;
  }

  crumb(CR_SEND);
  client.setTimeout(SEND_TIMEOUT_MS);                  // from here on we are sending

  const String method = reqLine.substring(0, sp1);
  const String url    = reqLine.substring(sp1 + 1, sp2);
  const int q = url.indexOf('?');
  const String path = (q >= 0) ? url.substring(0, q) : url;

  const char* result = "200";
  size_t want = 0, sent = 0;

  String body;                                         // POST body (config upload only, small)
  if (method == "POST" && contentLength > 0 && contentLength <= 8192) {
    body.reserve(contentLength);
    const uint32_t deadline = millis() + 2000;
    while (body.length() < contentLength && (int32_t)(millis() - deadline) < 0) {
      while (client.available() && body.length() < contentLength) body += (char)client.read();
      rp2040.wdt_reset();
      delay(1);
    }
  }

  if (path == "/api/config" && (method == "GET" || method == "POST")) {
    g_lastRequest = millis();
    if (method == "GET") {
      const String j = Cfg.json(true, true);           // passwords come out as ********
      want = j.length(); sent = sendText(client, "200 OK", "application/json; charset=utf-8", j);
    } else {
      String err; bool reboot = false;
      JsonDocument r;
      if (body.length() == 0)                         { r["ok"] = false; r["error"] = "empty body"; }
      else if (Cfg.applyJson(body, err, reboot))      { Cfg.applyRuntime(); r["ok"] = true; r["saved"] = Cfg.save(); r["reboot"] = reboot; }
      else                                            { r["ok"] = false; r["error"] = err; }
      String j; serializeJson(r, j);
      result = r["ok"].as<bool>() ? "200" : "400";
      want = j.length(); sent = sendText(client, r["ok"].as<bool>() ? "200 OK" : "400 Bad Request", "application/json; charset=utf-8", j);
    }
  } else if (method != "GET") {
    result = "405";
    const String body = "Only GET supported";
    want = body.length(); sent = sendText(client, "405 Method Not Allowed", "text/plain; charset=utf-8", body);
  } else if (path == "/" || path == "/index.html") {
    g_lastRequest = millis();
    want = sizeof(DASH_HTML) - 1;
    sendHead(client, "200 OK", "text/html; charset=utf-8", want);
    sent = writeAll(client, DASH_HTML, want);
  } else if (path == "/api/status") {
    g_lastRequest = millis();
    const String body = statusJson();
    want = body.length(); sent = sendText(client, "200 OK", "application/json; charset=utf-8", body);
  } else if (path == "/api/cli") {
    g_lastRequest = millis();
    String line;
    queryParam(url, "line", line);
    StringPrint out;
    Cli.execute(line, out);
    want = out.s.length(); sent = sendText(client, "200 OK", "text/plain; charset=utf-8", out.s);
  } else if (path == "/favicon.ico") {
    result = "204";
    sendHead(client, "204 No Content", "text/plain", 0);
  } else {
    result = "302";                                    // captive-portal probes and anything unknown
    redirectToRoot(client);
  }

  const bool peerConnected = client.connected();
  crumb(CR_CLOSE);
  client.flush();
  client.stop();

  Dg.httpRequests++;
  if (sent < want) Dg.httpFailedSend++;
  snprintf(g_lastReq, sizeof g_lastReq, "%s %s -> %s %u/%uB", method.c_str(), path.c_str(), result, (unsigned)sent, (unsigned)want);
  g_lastReqAt = millis();
  dlog("http %s %s %s -> %s, %u of %u bytes sent, %lu ms%s", who.c_str(), method.c_str(), url.c_str(), result,
       (unsigned)sent, (unsigned)want, (unsigned long)(millis() - t0),
       sent < want ? (peerConnected ? "  ** INCOMPLETE (peer still connected) **" : "  ** INCOMPLETE (peer gone) **") : "");
}

}  // namespace

// ───────────────────────── network ─────────────────────────

namespace { bool g_serverStarted = false; bool g_mdnsUp = false; }

IPAddress DashboardClass::ip() const { return g_apIP; }

bool DashboardClass::startAP() {
  const String ssid = String(WIFI_SSID_PREFIX) + Cfg.v.unitId;
  // Unit 1 uses channel 6 (the one the working vacuum-robot AP runs on); the others are spread 6/11/1.
  static const uint8_t channels[3] = {6, 11, 1};
  const uint8_t ch = Dg.channel ? Dg.channel : channels[(Cfg.v.unitId - 1) % 3];

  crumb(CR_AP_START);
  dlog("AP: starting SSID '%s' channel %u", ssid.c_str(), ch);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(AP_IP_OCTETS), IPAddress(AP_IP_OCTETS), AP_MASK);
  const bool ok = WiFi.softAP(ssid.c_str(), Cfg.v.apPass, ch, false);
  delay(200);                                  // let the AP and DHCP server settle before serving
  dlog("AP: softAP() returned %s", ok ? "true" : "FALSE");
  if (!ok) return false;

  g_apIP = WiFi.softAPIP();
  if (!g_apIP.isSet()) g_apIP = IPAddress(AP_IP_OCTETS);

  server.begin();
  g_serverStarted = true;
  if (!(Dg.flags & DIAG_NO_DNS)) g_dnsUp = dns.start(53, "*", g_apIP);
  dlog("AP: up, IP %s, http server on :80, DNS %s", g_apIP.toString().c_str(), g_dnsUp ? "on" : "OFF");
  return true;
}

// Router mode: join the dedicated furby network.
//
// arduino-pico has no truly asynchronous join: even beginNoBlock() waits inside the driver for up
// to the WiFi timeout. So:
//   * the first join happens in setup() with the full 15 s (nothing else is running yet)
//   * retries while running use a 4 s timeout, back off (10, 20, 30, then every 60 s), and widen
//     the watchdog around the call. With the router absent the main loop therefore stalls for at
//     most ~4 s per minute; the furby pulse engine on core 1 is not affected, and sensing,
//     standalone behaviour and furbies keep working without any WiFi.
void DashboardClass::staBegin() {
  const Settings& c = Cfg.v;
  const bool running = Dg.wdtOn;
  WiFi.setTimeout(running ? 4000 : 15000);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname((String("furbyhub") + c.unitId).c_str());
  IPAddress ip, gw, mask;
  if (c.ip[0] && ip.fromString(c.ip)) {              // set before EVERY join: begin() tears the interface down first
    if (!(c.gateway[0] && gw.fromString(c.gateway))) gw = IPAddress(ip[0], ip[1], ip[2], 1);
    if (!(c.mask[0] && mask.fromString(c.mask))) mask = IPAddress(255, 255, 255, 0);
    WiFi.config(ip, gw, gw, mask);                   // address, dns, gateway, subnet mask
  }
  dlog("STA: joining '%s' (%s), attempt %u", c.staSsid, c.ip[0] ? "static IP" : "DHCP", (unsigned)_staFails + 1);
  const uint32_t t0 = millis();
  if (running) rp2040.wdt_begin(20000);
  WiFi.beginNoBlock(c.staSsid, c.staPass[0] ? c.staPass : nullptr);
  if (running) rp2040.wdt_begin(WATCHDOG_MS);
  dlog("STA: join attempt took %lu ms", (unsigned long)(millis() - t0));
  if (_staFails < 255) _staFails++;
}

void DashboardClass::restartAP() {
  crumb(CR_AP_RESTART);
  dlog("AP: restarting");
  server.stop();
  if (g_dnsUp) { dns.stop(); g_dnsUp = false; }
  WiFi.softAPdisconnect(true);
  delay(80);
  _apUp = startAP();
}

void DashboardClass::begin() {
  _sta = !strcmp(Cfg.v.netMode, "sta") && Cfg.v.staSsid[0];
  _stations = 0;
  _nextStationPoll = millis() + 3000;
  _lastRequestMs = _lastClientMs = millis();
  _nextApRetryMs = millis() + 5000;
  if (_sta) {
    _apUp = false;
    staBegin();
    _nextLinkCheck = millis() + 500;
    _staNextTry = millis() + 10000;
  } else {
    _apUp = startAP();
  }
}

// softAPgetStationNum() makes raw CYW43 driver calls (plus a malloc) without taking the
// WiFi lock. Calling it on every loop() pass raced the driver, which is when a client joins.
// So: ask rarely, and under the same lock the WiFi stack itself uses.
void DashboardClass::refreshStations() {
  if (_sta || !_apUp || (Dg.flags & DIAG_NO_STATIONPOLL)) { _stations = 0; return; }
  crumb(CR_STATIONS);
  cyw43_arch_lwip_begin();
  const uint8_t n = WiFi.softAPgetStationNum();
  cyw43_arch_lwip_end();
  if (n != _stations) dlog("wifi: clients %u -> %u", _stations, n);
  _stations = n;
}

void DashboardClass::printNet(Print& out) const {
  if (_sta) {
    int rssi = 0;
    if (_apUp) { cyw43_arch_lwip_begin(); rssi = WiFi.RSSI(); cyw43_arch_lwip_end(); }
    out.printf("wifi: router mode, '%s' - %s  IP %s  signal %d dBm  (dashboard: http://furbyhub%u.local or the IP)\r\n", Cfg.v.staSsid,
               _apUp ? "CONNECTED" : "NOT CONNECTED (retrying with back-off, at most once a minute)", g_apIP.toString().c_str(), rssi, Cfg.v.unitId);
  } else {
    out.printf("wifi AP: %s  SSID %s%u  IP %s  clients: %u%s\r\n", _apUp ? "up" : "DOWN", WIFI_SSID_PREFIX, Cfg.v.unitId,
               g_apIP.toString().c_str(), _stations, (Dg.flags & DIAG_NO_STATIONPOLL) ? " (polling disabled)" : "");
  }
  out.printf("http: %lu requests, %lu empty connections, %lu incomplete sends\r\n",
             (unsigned long)Dg.httpRequests, (unsigned long)Dg.httpEmpty, (unsigned long)Dg.httpFailedSend);
  out.printf("last request: %s  (%lu s ago)\r\n", g_lastReq, g_lastReqAt ? (unsigned long)((millis() - g_lastReqAt) / 1000) : 0UL);
}

void DashboardClass::loop() {
  const uint32_t now = millis();

  if (_sta) {
    if ((int32_t)(now - _nextLinkCheck) >= 0) {
      _nextLinkCheck = now + 1000;
      cyw43_arch_lwip_begin();
      const bool up = (WiFi.status() == WL_CONNECTED);
      cyw43_arch_lwip_end();
      if (up && !_apUp) {
        _apUp = true;
        g_apIP = WiFi.localIP();
        if (!g_serverStarted) { server.begin(); g_serverStarted = true; }
        dlog("STA: connected, IP %s", g_apIP.toString().c_str());
        if (!g_mdnsUp) {
          const String host = String("furbyhub") + Cfg.v.unitId;
          g_mdnsUp = MDNS.begin(host.c_str());
          if (g_mdnsUp) { MDNS.addService("http", "tcp", 80); dlog("mDNS: http://%s.local", host.c_str()); }
        }
      } else if (!up && _apUp) {
        _apUp = false;
        _staNextTry = now + 3000;
        dlog("STA: link lost, rejoining");
      }
      if (up) _staFails = 0;
      if (!up && (int32_t)(now - _staNextTry) >= 0) {
        staBegin();
        static const uint32_t backoff[] = {10000, 20000, 30000, 60000};
        _staNextTry = millis() + backoff[_staFails < 4 ? _staFails - 1 : 3];
      }
    }
    if (_apUp) { if (g_mdnsUp) MDNS.update(); handleClient(); }
    return;
  }

  if (!_apUp) {                       // AP failed to come up: keep trying, the unit still works standalone
    if ((int32_t)(now - _nextApRetryMs) >= 0) {
      restartAP();
      _nextApRetryMs = now + 5000;
    }
    return;
  }

  if (g_dnsUp) { crumb(CR_DNS); dns.processNextRequest(); }
  handleClient();

  if ((int32_t)(now - _nextStationPoll) >= 0) {
    _nextStationPoll = now + 3000;
    refreshStations();
  }

  // Unattended installation: if nobody has been connected for a long time, rebuild the AP
  // (the original single-user remote did the same as a cheap cure for a stuck CYW43 AP).
  // NB: take a FRESH millis() here. `now` above was read before handleClient(), which stamps
  // g_lastRequest slightly later - subtracting would wrap around and restart the AP after
  // every single request.
  if (g_lastRequest) _lastRequestMs = g_lastRequest;
  const uint32_t t = millis();
  if (_stations > 0) _lastClientMs = t;
  const uint32_t lastActivity = ((int32_t)(_lastRequestMs - _lastClientMs) > 0) ? _lastRequestMs : _lastClientMs;
  const int32_t idle = (int32_t)(t - lastActivity);
  if (idle > (int32_t)APRESTART_IDLE_MS) {
    dlog("AP: idle for %ld s with no clients, rebuilding it", (long)(idle / 1000));
    restartAP();
    _lastRequestMs = _lastClientMs = millis();
  }
}
