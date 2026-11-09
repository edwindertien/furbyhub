// reFurbyshment - one unit: 16 furbies, WiFi dashboard, standalone behaviour, CLI.
//
//   core 0  setup()/loop()    WiFi AP + dashboard, CLI, standalone state machine
//   core 1  setup1()/loop1()  FurbyBus pulse engine (hard timing, never blocked by WiFi)
#include <Arduino.h>
#include "config.h"
#include "Settings.h"
#include "FurbyBus.h"
#include "Standalone.h"
#include "Dashboard.h"
#include "Cli.h"
#include "Diag.h"
#include "Osc.h"
#include "Sensor.h"

void setup() {
  diagCaptureReset();          // what happened last time? (watchdog? which stage?)
  Serial.begin(115200);

  Cfg.load();
  diagLoad();                  // diagnostic switches (see `diag` in the CLI)

  // Recovery: hold the button while powering up for 3 s = network settings back to AP-mode defaults
  // (so a wrong router name or password can never lock you out of a hub).
  bool netReset = false;
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(5);
  if (digitalRead(PIN_BUTTON) == LOW) {
    const uint32_t t0 = millis();
    while (digitalRead(PIN_BUTTON) == LOW && millis() - t0 < 3000) delay(10);
    if (millis() - t0 >= 3000) { Cfg.resetNetwork(); Cfg.save(); netReset = true; }
  }

  // Bring the wires up first (floating) and release core 1, before WiFi takes its time.
  BusTiming t{Cfg.v.pulseUs, Cfg.v.halfBitUs, Cfg.v.gapMs, Cfg.v.wakeMs, Cfg.v.repeats};
  FurbyBus.begin(t);
  dlog("config: %s%s", Cfg.loadNote.c_str(), netReset ? "  [button held: network reset to AP defaults]" : "");

  if (!(Dg.flags & DIAG_NO_LED)) pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(micros() ^ (uint32_t)rp2040.getCycleCount());

  Sensor.begin();
  Standalone.begin();
  Dashboard.begin();
  Osc.begin();
  Cli.begin();

  if (!(Dg.flags & DIAG_NO_WDT)) { rp2040.wdt_begin(WATCHDOG_MS); Dg.wdtOn = true; }   // unattended installation: reboot if core 0 ever hangs
  dlog("boot done, unit %u, flags=0x%02x", Cfg.v.unitId, Dg.flags);
}

void loop() {
  if (!(Dg.flags & DIAG_NO_WDT)) rp2040.wdt_reset();
  crumb(CR_CLI);
  Cli.poll();
  crumb(CR_STANDALONE);
  if (!(Dg.flags & DIAG_NO_INPUTS)) Sensor.loop();
  Standalone.loop();
  crumb(CR_LOOP);
  Dashboard.loop();
  crumb(CR_OSC);
  Osc.loop();

  // LED: steady while furbies are awake, short blink when idle, double blink if the AP is down
  static uint32_t lastLed = 0;
  if (!(Dg.flags & DIAG_NO_LED) && millis() - lastLed >= 50) {
    lastLed = millis();
    const uint32_t ph = millis() % 2000;
    bool on;
    if (!Dashboard.apUp())                              on = (ph < 100) || (ph > 300 && ph < 400);
    else if (Standalone.state() != StandaloneClass::Idle) on = true;
    else                                                on = ph < 60;
    static int lastOn = -1;               // on a Pico W the LED lives on the WiFi chip: only touch it on change
    if (lastOn != (int)on) { lastOn = on; digitalWrite(LED_BUILTIN, on); }
  }
}

// ── core 1 ──
void setup1() {
  while (!FurbyBus.ready()) delay(1);   // core 1 is launched before setup() runs on core 0
  FurbyBus.coreSetup();
  // `diag off core1` (bisecting the WiFi problem): park this core instead of running the engine
}

void loop1() {
  if (!Dg.core1Enabled) { delay(500); return; }
  FurbyBus.coreTick();
}
