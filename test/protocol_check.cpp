// Host-side check: does the slot-based generator in FurbyProtocol.h emit the
// same pulse pattern as the original FurbyCentralMegaADK::sendNibble()?
//
//   g++ -std=c++17 -I include test/protocol_check.cpp -o /tmp/protocol_check && /tmp/protocol_check
#include <cstdio>
#include <vector>
#include "FurbyProtocol.h"

// --- original sketch, transcribed: returns pulse start times in microseconds
static std::vector<int> originalNibble(int value) {
  std::vector<int> t;
  int now = 0;
  auto zero = [&]() { t.push_back(now); now += 150 + 1900; };
  auto one  = [&]() { t.push_back(now); now += 150 + 900; t.push_back(now); now += 150 + 900; };
  one();                                               // start bit
  for (int n = 0; n < 4; n++) { if (value & 1 << n) one(); else zero(); }
  for (int n = 0; n < 4; n++) { if (value & 1 << n) zero(); else one(); }
  return t;
}

// --- new generator: pulse start slot indices
static std::vector<int> newNibble(int value) {
  std::vector<int> s;
  for (int slot = 0; slot < FurbyProtocol::SLOTS_PER_PACKET; slot++)
    if (FurbyProtocol::pulseInSlot((uint8_t)value, (uint8_t)slot)) s.push_back(slot);
  return s;
}

int main() {
  int bad = 0;
  for (int v = 0; v < 16; v++) {
    auto a = originalNibble(v);
    auto b = newNibble(v);
    bool same = a.size() == b.size();
    // Compare timing within tolerance: original bit = 2050..2100 us, new slot = 1050 us
    for (size_t i = 0; same && i < a.size(); i++) {
      double newT = b[i] * 1050.0;
      double err  = newT - a[i];
      if (err < 0) err = -err;
      if (err > 9 * 50 + 1) same = false;   // worst-case drift: nine '0' bits are 50 us short each
    }
    printf("code %2d (%c): %2zu pulses original, %2zu new  %s\n",
           v, FurbyProtocol::letterFor(v), a.size(), b.size(), same ? "OK" : "MISMATCH");
    if (!same) bad++;
  }
  // 'sleep' per the sketch notes: four ones -> 1 1111 0000
  printf("\nbits for sleep (15): ");
  for (int k = 0; k < 9; k++) printf("%d", FurbyProtocol::bitValue(15, k));
  printf("   (expect 111110000)\n");
  printf("%s\n", bad ? "FAILED" : "all 16 codes match the original sketch");
  return bad ? 1 : 0;
}
