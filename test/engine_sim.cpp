// Host simulation of the REAL FurbyBus.cpp (compiled unmodified) against a fake
// GPIO + simulated microsecond clock. It records every pin transition, then checks
// the waveform against the original FurbyCentralMegaADK timing.
//
//   cd test && ./run_host_tests.sh
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include "FurbyBus.h"
#include "FurbyCodes.h"
#include "FurbyProtocol.h"

// ───────────── simulated hardware ─────────────
uint64_t g_sim_us = 0;
enum PinState : uint8_t { Z = 0, LOW_ = 1, HIGH_ = 2 };

static bool g_dirOut[32], g_latch[32];
static PinState g_cur[32];
struct Ev { uint64_t t; unsigned pin; PinState s; };
static std::vector<Ev> g_ev;

static void settle(unsigned pin) {
  PinState s = g_dirOut[pin] ? (g_latch[pin] ? HIGH_ : LOW_) : Z;
  if (s != g_cur[pin]) { g_cur[pin] = s; g_ev.push_back({g_sim_us, pin, s}); }
}
void gpio_init(unsigned pin)            { g_dirOut[pin] = false; g_latch[pin] = false; settle(pin); }
void gpio_put(unsigned pin, bool v)     { g_latch[pin] = v; settle(pin); }
void gpio_set_dir(unsigned pin, bool o) { g_dirOut[pin] = o; settle(pin); }
void gpio_disable_pulls(unsigned)       {}
uint64_t time_us_64()                   { return g_sim_us; }
void busy_wait_us_32(uint32_t us)       { g_sim_us += us; }
void tight_loop_contents()              { g_sim_us += 1; }

// ───────────── analysis helpers ─────────────
struct Pulse { uint64_t start, width; };
static std::vector<Pulse> pulsesOf(unsigned pin) {
  std::vector<Pulse> p; bool hi = false; uint64_t t0 = 0;
  for (auto& e : g_ev) if (e.pin == pin) {
    if (e.s == HIGH_ && !hi) { hi = true; t0 = e.t; }
    else if (e.s != HIGH_ && hi) { hi = false; p.push_back({t0, e.t - t0}); }
  }
  return p;
}
static PinState stateAt(unsigned pin, uint64_t t) {
  PinState s = Z;
  for (auto& e : g_ev) if (e.pin == pin && e.t <= t) s = e.s;
  return s;
}
static void runFor(uint64_t us) {
  const uint64_t end = g_sim_us + us;
  while (g_sim_us < end) FurbyBus.coreTick();
}
static void resetLog() { g_ev.clear(); }

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)
#define OK(...) do { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } while (0)

// original sketch model (pulse start times, us) for one nibble
static std::vector<int> originalNibble(int value, int* totalUs) {
  std::vector<int> t; int now = 0;
  auto zero = [&]() { t.push_back(now); now += 150 + 1900; };
  auto one  = [&]() { t.push_back(now); now += 150 + 900; t.push_back(now); now += 150 + 900; };
  one();
  for (int n = 0; n < 4; n++) { if (value & 1 << n) one(); else zero(); }
  for (int n = 0; n < 4; n++) { if (value & 1 << n) zero(); else one(); }
  *totalUs = now;
  return t;
}

int main() {
  BusTiming t{150, 1050, 90, 100, 6};
  FurbyBus.begin(t);
  FurbyBus.coreSetup();

  // ── 1. message waveform vs the original sketch ──────────────────────────
  printf("1. message 'dance' (code 5), 2 repeats, furby 1 (GP0)\n");
  resetLog();
  FurbyBus.send(0, 5, 2);
  runFor(400000);
  auto p = pulsesOf(FURBY_PIN[0]);
  CHECK(p.size() == 28, "expected 28 pulses (2 x 14), got %zu", p.size());
  if (p.size() == 28) OK("28 pulses (2 packets x 14)");
  bool widthOk = true; for (auto& x : p) if (x.width < 145 || x.width > 160) widthOk = false;
  CHECK(widthOk, "pulse widths outside 150us +-5/10");
  if (widthOk) OK("every pulse is 150 us wide");
  int origTotal; auto orig = originalNibble(5, &origTotal);
  if (p.size() >= 14) {
    const uint64_t base = p[0].start;
    double maxErr = 0;
    for (size_t i = 0; i < 14; i++) maxErr = std::max(maxErr, std::fabs((double)(p[i].start - base) - orig[i]));
    CHECK(maxErr < 500, "pulse positions deviate from original by %.0f us", maxErr);
    OK("pulse positions within %.0f us of the original sketch's (original has 50 us/bit drift on zeros)", maxErr);
  }
  if (p.size() >= 15) {
    const double period = (double)(p[14].start - p[0].start) / 1000.0;
    const double origPeriod = (origTotal + 90000) / 1000.0;
    CHECK(std::fabs(period - origPeriod) < 2.0, "repeat period %.2f ms vs original %.2f ms", period, origPeriod);
    OK("repeat period %.2f ms (original sketch: %.2f ms)", period, origPeriod);
  }
  // wire state: low between pulses inside the packet, floating in the gap, floating when finished
  CHECK(stateAt(FURBY_PIN[0], p[0].start + 500) == LOW_, "wire should be driven LOW between pulses");
  CHECK(stateAt(FURBY_PIN[0], p[0].start + 40000) == Z,  "wire should float during the inter-packet gap");
  CHECK(stateAt(FURBY_PIN[0], g_sim_us) == Z,            "wire should float when idle");
  OK("LOW between pulses, high-Z in the gap and when idle");
  CHECK(FurbyBus.status(0).lastCode == 5 && !FurbyBus.status(0).busy, "status after message");

  // ── 2. wake pulse ───────────────────────────────────────────────────────
  printf("2. wake pulse, furby 2 (GP1)\n");
  resetLog();
  FurbyBus.wake(1);
  runFor(300000);
  p = pulsesOf(FURBY_PIN[1]);
  CHECK(p.size() == 1, "expected one long pulse, got %zu", p.size());
  if (p.size() == 1) {
    CHECK(p[0].width > 99000 && p[0].width < 103000, "wake width %llu us", (unsigned long long)p[0].width);
    OK("one HIGH pulse of %.1f ms (original: 100 ms), then high-Z", p[0].width / 1000.0);
  }
  CHECK(stateAt(FURBY_PIN[1], g_sim_us) == Z, "wire floats after wake");
  CHECK(FurbyBus.status(1).assumed == ASSUME_AWAKE, "assumed state awake");

  // ── 3. parallel channels are independent ────────────────────────────────
  printf("3. 16 furbies at once: wake 1..8, message to 9..16\n");
  resetLog();
  for (int i = 0; i < 8; i++)  FurbyBus.wake(i);
  for (int i = 8; i < 16; i++) FurbyBus.send(i, 8, 1);
  runFor(300000);
  bool all = true;
  for (int i = 0; i < 8; i++)  { auto q = pulsesOf(FURBY_PIN[i]); if (q.size() != 1) all = false; }
  for (int i = 8; i < 16; i++) { auto q = pulsesOf(FURBY_PIN[i]); if (q.size() != 14) all = false; }
  CHECK(all, "not every channel produced the expected waveform");
  if (all) OK("all 16 channels produced correct, simultaneous waveforms");

  // ── 4. sleep macro = message, 800 ms, message (as original) ─────────────
  printf("4. sleep macro, furby 3 (GP2)\n");
  resetLog();
  FurbyBus.sleep(2);
  runFor(3000000);
  p = pulsesOf(FURBY_PIN[2]);
  CHECK(p.size() == 2 * 6 * 14, "expected 168 pulses, got %zu", p.size());
  if (p.size() == 168) {
    const double gapMs = (double)(p[84].start - p[83].start) / 1000.0;
    CHECK(gapMs > 800 && gapMs < 1000, "pause between the two sleep messages %.0f ms", gapMs);
    OK("168 pulses = 2 x 6 x 14, %.0f ms between the two messages", gapMs);
  }
  CHECK(FurbyBus.status(2).assumed == ASSUME_ASLEEP, "assumed asleep");

  // ── 5. clear aborts immediately and floats the wire ─────────────────────
  printf("5. clear() in the middle of a long message, furby 5 (GP4)\n");
  resetLog();
  FurbyBus.send(4, 3, 50);
  runFor(300000);
  FurbyBus.clear(4);
  const uint64_t tc = g_sim_us;
  runFor(300000);
  auto q = pulsesOf(FURBY_PIN[4]);
  size_t late = 0; for (auto& x : q) if (x.start > tc + 3000) late++;
  CHECK(late == 0, "%zu pulses after clear()", late);
  CHECK(stateAt(FURBY_PIN[4], g_sim_us) == Z, "wire floats after clear");
  OK("no pulses after clear(), wire floating");

  // ── 6. level hold ───────────────────────────────────────────────────────
  printf("6. level HIGH held until cleared, furby 6 (GP5)\n");
  resetLog();
  FurbyBus.level(5, true, 0xFFFF);
  runFor(500000);
  CHECK(stateAt(FURBY_PIN[5], g_sim_us) == HIGH_, "should still be HIGH");
  FurbyBus.clear(5); runFor(5000);
  CHECK(stateAt(FURBY_PIN[5], g_sim_us) == Z, "should float after clear");
  OK("held HIGH, then released to high-Z");

  // ── 7. queue ordering: wake then message on the same furby ──────────────
  printf("7. wake followed by message queued on the same furby (GP6)\n");
  resetLog();
  FurbyBus.wake(6); FurbyBus.send(6, 0, 1);
  runFor(500000);
  p = pulsesOf(FURBY_PIN[6]);
  CHECK(p.size() == 15 && p[0].width > 99000 && p[1].width < 200, "long pulse then 14 short ones expected (got %zu)", p.size());
  if (p.size() == 15) {
    const double floatMs = (double)(p[1].start - (p[0].start + p[0].width)) / 1000.0;
    CHECK(floatMs >= 45.0, "wire must float after the long pulse before the message (got %.1f ms)", floatMs);
    OK("long wake pulse, %.1f ms floating, then the 14-pulse message", floatMs);
  }

  // ── 8. tick stability with everything busy ──────────────────────────────
  printf("8. tick period with all 16 channels sleeping\n");
  resetLog();
  for (int i = 0; i < 16; i++) FurbyBus.sleep(i);
  const uint64_t t0 = g_sim_us;
  runFor(1000000);
  std::vector<uint64_t> starts; for (auto& x : pulsesOf(FURBY_PIN[0])) starts.push_back(x.start);
  // consecutive pulses within one packet are whole multiples of 1050 us apart
  bool aligned = starts.size() > 10;
  for (size_t i = 1; i < 13 && i < starts.size(); i++) if ((starts[i] - starts[0]) % 1050 > 1) aligned = false;
  CHECK(aligned, "pulses are not on the 1050 us grid");
  OK("pulses sit on the 1050 us grid with 16 channels busy (t0=%llu)", (unsigned long long)t0);

  printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL ENGINE CHECKS PASSED", g_fail, g_fail == 1 ? "" : "s");
  return g_fail ? 1 : 0;
}
