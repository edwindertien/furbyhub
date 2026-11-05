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

void setup() {
  Serial.begin(115200);

  Cfg.load();

  // Bring the wires up first (floating) and release core 1, before WiFi takes its time.
  BusTiming t{Cfg.v.pulseUs, Cfg.v.halfBitUs, Cfg.v.gapMs, Cfg.v.wakeMs, Cfg.v.repeats};
  FurbyBus.begin(t);

  pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(micros() ^ (uint32_t)rp2040.getCycleCount());

  Standalone.begin();
  Dashboard.begin();
  Cli.begin();

  rp2040.wdt_begin(WATCHDOG_MS);   // unattended installation: reboot if core 0 ever hangs
}

void loop() {
  rp2040.wdt_reset();
  Cli.poll();
  Standalone.loop();
  Dashboard.loop();

  // LED: steady while furbies are awake, short blink when idle, double blink if the AP is down
  static uint32_t lastLed = 0;
  if (millis() - lastLed >= 50) {
    lastLed = millis();
    const uint32_t ph = millis() % 2000;
    bool on;
    if (!Dashboard.apUp())                              on = (ph < 100) || (ph > 300 && ph < 400);
    else if (Standalone.state() != StandaloneClass::Idle) on = true;
    else                                                on = ph < 60;
    digitalWrite(LED_BUILTIN, on);
  }
}

// ── core 1 ──
void setup1() {
  while (!FurbyBus.ready()) delay(1);   // core 1 is launched before setup() runs on core 0
  FurbyBus.coreSetup();
}

void loop1() {
  FurbyBus.coreTick();
}
