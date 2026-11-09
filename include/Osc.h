#pragma once
#include <Arduino.h>
#include "OscCodec.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Hub-to-hub messaging over OSC/UDP on the shared router network. No hub is special:
// every hub broadcasts what its sensor sees and decides for itself what to do with
// what the others report (rules in furbyhub.json).
//
// A hub sends (to the subnet broadcast address, or network.osc_target):
//   /furby/hub/<id>/presence  i        0/1, on every change
//   /furby/hub/<id>/distance  i        mm, when it moved >= 25 mm (max 10 per second)
//   /furby/hub/<id>/status    iiii     presence, distance mm (-1 none), standalone state, uptime s   (every 2 s)
//
// A hub accepts commands on network.osc_port:
//   /furby/hub/<id|all>/<command> [args...]   command = wake sleep send wakeall sleepall trigger stop
//   the arguments are appended to the CLI command:  /furby/hub/2/send "1-4" 5   ==   send 1-4 5
//
// There is no authentication: anyone on the network can send these. Keep the furby network private.
// ---------------------------------------------------------------------------

constexpr uint8_t MAX_PEERS = 8;

struct PeerInfo {
  bool     used = false;
  uint8_t  id = 0;
  int8_t   presence = -1;       // -1 = not known yet
  int32_t  dist = -1;
  uint8_t  state = 0;
  uint32_t uptime = 0;
  uint32_t lastMs = 0;
  uint32_t ip = 0;
};

class OscClass {
public:
  void begin();
  void loop();

  bool    active() const { return _active; }
  const PeerInfo& peer(uint8_t i) const { return _peers[i]; }
  bool    send(const char* address, const OscCodec::Arg* args, uint8_t n);
  void    printStatus(Print& out) const;

private:
  void startUdp();
  void receive();
  void handlePacket(const uint8_t* b, size_t n, uint32_t fromIp);
  void peerEvent(uint8_t id, uint32_t ip, const char* what, int32_t value);
  PeerInfo* peerSlot(uint8_t id);
  void runCommand(const char* cmd, const OscCodec::Msg& m);
  void fireRules(uint8_t id, const char* what, int32_t prev, int32_t cur);
  void sendEvent(const char* what, int32_t v);
  void runPending();

  struct Pending { bool used = false; uint32_t due = 0; char line[48] = {0}; };

  bool     _active = false;
  uint32_t _target = 0;                 // IPAddress as uint32
  PeerInfo _peers[MAX_PEERS];
  Pending  _pending[8];
  uint32_t _lastFire[8] = {0};
  bool     _firstSend = true, _lastPres = false;
  int32_t  _lastDist = -100000;
  uint32_t _lastDistAt = 0, _nextBeat = 0;
  uint32_t _sent = 0, _received = 0, _bad = 0, _ruleHits = 0;
};

extern OscClass Osc;
