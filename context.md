# reFurbyshment - technical context and handoff

Companion to [README.md](README.md). The README says **what it is and how to use it**; this document says **how it got here, why it is built this way, what is proven, and what to do next**. It is written so that someone (or some AI session) starting cold can continue the work without the conversation history.

Version **0.1.0**, first version. Written on 2026-11-08.

## 1. Summary

A distributed controller for 1999-style Furbies. Five Raspberry Pi **Pico W** hubs, each running up to **16 Furbies**, one wire per Furby, replace one Arduino Mega with a central wiring hub. Each Furby carries a single modification: a wire in parallel with its tilt sensor and its IR receiver. A long HIGH pulse fakes the tilt sensor (wake); short pulses carry the original infrared protocol (9-bit packets).

Each hub is autonomous (presence sensor or button, timers, random chatter), has a WiFi dashboard and a serial CLI, stores everything in one `furbyhub.json` on LittleFS, and can talk to the other hubs over OSC on a dedicated router.

Stack: PlatformIO, earlephilhower arduino-pico, VS Code. About 3.6 kLOC of firmware (`src/` + `include/`, of which the embedded web page is 485 lines), 5 host test suites.

## 2. State at handoff

| | |
|---|---|
| Firmware | builds with no warnings under `-Wall -Wextra`; RAM 76 240 B (29.1 %), flash 478 376 B (30.5 % of the 1.5 MB program partition) |
| Config file | `/furbyhub.json`, 0.5 MB LittleFS partition |
| Host tests | all pass (protocol, engine simulation, presence logic, OSC + rules, config layer) |

**Proven on hardware** (reported by the author): Furby control from the CLI over the 3.3 V wire; WiFi AP + captive portal + dashboard load and stay up; `furbyhub.json`, looks and names persist across a reboot.

**Written, compiled, tested only on the host, never run on a board:** PIR and HC-SR04 drivers, router (STA) mode, mDNS, OSC over real UDP, rules, the Santa hat and mane drawing in a real browser, the Looks editor's POST path in a real browser.

**Unresolved by design:** whether Furbies are happy at 3.3 V logic across all 16 (it works for the tested one), the 6 V supply route, independent confirmation of the packet structure.

## 3. Environment and toolchain

```
platform  = https://github.com/maxgerhardt/platform-raspberrypi.git
board     = rpipicow          board_build.core = earlephilhower
framework = arduino
board_build.filesystem = littlefs     board_build.filesystem_size = 0.5m
lib_deps  = https://github.com/bblanchon/ArduinoJson.git#v7.2.1     (git URL, pinned tag)
build_flags = -O2
```

- Developer machine: macOS (`/Users/.../.platformio`). The AI-side development environment had **no hardware** and could not reach the PlatformIO registry, only GitHub; that is why ArduinoJson is a pinned git URL and why host-side tests carry so much weight.
- **Never copy anything from `test/` into `src/` or `include/`.** `test/mock_cfg/Arduino.h` shadows the real framework header and produces hundreds of unrelated errors. This happened once.
- A first upload needs BOOTSEL. LittleFS survives re-flashing the firmware; to wipe it, erase flash.

### arduino-pico facts learned the hard way

These were verified by reading the framework source (`~/.platformio/packages/framework-arduinopico`), not assumed.

| Fact | Consequence |
|---|---|
| Core 1 is **launched before `setup()`** runs on core 0 | `setup1()` must wait on `FurbyBus.ready()` |
| `WiFi.begin()` **blocks** up to twice `_timeout` (15 s) while not joined; `beginNoBlock()` still blocks inside the driver's join loop | router join cannot be truly asynchronous; see 4.9 |
| `WiFi.softAPgetStationNum()` does raw CYW43 calls plus a `malloc`, **unlocked** | never call it per loop pass; cached and wrapped in `cyw43_arch_lwip_begin/end` |
| `WebServer` serves **one connection at a time** and holds it up to 5 s (`HTTP_MAX_CLOSE_WAIT`) | unusable for phones that open several sockets; replaced by a raw `WiFiServer` |
| `WiFiClient::connected()` is false after the peer half-closes (FIN) even though it still wants the reply | never gate sending on `connected()` |
| `WiFi.config()` takes `(ip, dns, gateway, subnet)`; `begin()` tears the interface down first | static IP is set before **every** join |
| `rp2040.wdt_begin()` may be called again to change the timeout | used to widen the watchdog around a join |
| Flash writes (LittleFS, EEPROM) idle the other core | saves are explicit and rare; the engine resyncs after a late tick |
| ArduinoJson 7 stores a plain `const char*` **by pointer** | strings must be copied into the document (`JsonString(s, JsonString::Copied)`), see 4.7 |

## 4. The technical route

Chronological, with the reasoning. Each item: what was decided, why, and what it cost or taught.

### 4.1 Starting point

Inputs: the **FurbyCentralMegaADK** sketch (one Mega driving all Furbies, 60 s awake time, sleep sent twice 800 ms apart, 9-bit packets sent six times) and a **vacuum-robot WiFi remote** project (`WIFIremote`: Pico W AP + captive portal + a raw `WiFiServer` landing page) as the proven UI/AP pattern. Goal: five Pico W hubs, 16 Furbies each, Waveshare **Pico-Servo-Driver** for connectivity, standalone plus web dashboard plus a CLI "so everything can be tested", especially the 3.3 V behaviour of the wire hack.

The Waveshare pinout was only available as an image that was never seen; the schematic's netlist text shows four 4-pin signal headers (P2-P5) carrying **GP0-GP15**. That mapping is an inference, isolated in one table (`FURBY_PIN[]`).

The protocol was taken from the sketch. Two searches found only the 2012 audio-protocol Furby and a Furby source-code dump, nothing confirming the 1999 IR framing. The sketch's header cites an early reverse-engineering page (`veg.nildram.co.uk/furby.htm`) that could not be fetched. **The structure is therefore "works in the author's sketch", not "independently confirmed".**

### 4.2 The pulse engine on core 1

**Decision.** One engine on core 1, ticking every half-bit time (1050 us), stepping all 16 Furbies each tick; core 0 talks to it through a lock-free `queue_t` (64 actions) of small actions (Wake, Msg, Wait, Level) with a per-Furby FIFO of 8.

**Why.** Packets need about 150 us pulses on a ~1 ms grid for ~110 ms at a time; WiFi, HTTP and flash on core 0 must never disturb that. A single tick for all 16 Furbies means 16 Furbies cost the same as one, and `delay()`-based code would serialise them. Tick 1050 us reproduces the original sketch (a `1` bit is 2100 us; a `0` is 2050 us in the original, 2100 us here, within the receiver's tolerance).

**Idle state.** A wire is driven only while something happens on it; otherwise it is a floating input with no pulls (like the sketch's `pinMode(INPUT)`), so it cannot disturb the Furby's own tilt sensor or receiver.

**Verification.** The real `FurbyBus.cpp` is compiled on the host against a fake clock and GPIO and checked against a transcription of the original sketch's timing.

**Found by that simulation:** a wake pulse immediately followed by a message on the same Furby merged into one long pulse, with no floating gap. The sketch's notes say the tilt trigger is "a long pulse followed by a floating state". Fix: a mandatory 50 ms float after Wake and Level actions.

**Also:** core 1 starts before `setup()`; the `ready()` handshake exists because of that.

### 4.3 Standalone behaviour

State machine Idle, Waking, Awake, Sleeping (see README). Wake-up is staggered (default 300 ms) because sixteen motors on one regulator is the main current risk. Chatter only uses codes 0, 5, 8, 11 and only targets Furbies assumed awake and idle, because a speaking Furby ignores packets. A start-up cooldown of 2 s ignores PIR start-up pulses.

### 4.4 WiFi bring-up: four bugs, one method

This took the most effort. **Instrumentation first** is what finally worked, see 4.5.

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | Phone drops immediately on connecting | `softAPgetStationNum()` called every loop pass, unlocked, racing the CYW43 driver | cache it, poll every 3 s under `cyw43_arch_lwip_begin/end` |
| 2 | Captive portal opens but the page is empty | `WebServer` handles one connection and waits up to 5 s for it to close; phones open several sockets, the real request queued behind idle ones and timed out | raw `WiFiServer`: accept, reply in 512-byte chunks, flush, close. The vacuum-robot project had been doing exactly this |
| 3 | Disconnect right after every request | my "nobody around for 10 min, rebuild the AP" check read `now` **before** `handleClient()`, which stamped `g_lastRequest` a few ms **later**; `now - last` wrapped to ~4e9 and restarted the AP after every request | fresh `millis()` and a signed difference. This bug existed in every earlier version, so it was behind the very first symptom too |
| 4 | Pages truncated at a multiple of 512 bytes | send loop guarded by `c.connected()`; a half-closing client looks "not connected" while still waiting | send until done or 2 s without progress; log `peer still connected / gone` on a short send |

Lessons: (a) compare against the working project line by line before theorising; (b) unsigned time arithmetic must use signed differences and a *fresh* clock reading; (c) a "self-healing" watchdog is also a way to kill the thing it protects, so make it log why it fired.

### 4.5 Diagnostics as a feature

When the cause was unclear, `Diag` was added and kept: `dlog()` live log of every connection and request (bytes sent, duration, `INCOMPLETE` with peer state), `crumb()` stages stored in a **watchdog scratch register** that survives a watchdog reset (so `net` can say "hung in `http send`"), reset-cause capture, `net`/`debug`/`diag`, and **runtime switches** to disable core 1, the watchdog, the LED, station polling, input polling or DNS from the CLI, stored in flash and applied at next boot, so a WiFi problem can be bisected without reflashing. Keep this module.

### 4.6 Settings: EEPROM to LittleFS JSON

**Decision.** `/furbyhub.json` on LittleFS is the single source of truth; the in-RAM `Settings` struct is just its form. CLI `config get/set`, the web editor and file upload all go through the same `applyJson()`/`setPath()`; dotted paths are **generated from the JSON itself**, so there is no per-key code to drift.

**Why.** The author wanted stored, editable hub config, sensor thresholds and network credentials from CLI and web (and, for the project site, a readable file).

**Properties.** Passwords redacted on read and preserved when `********` is written back; validation is all-or-nothing (the struct is restored on any error); save writes `furbyhub.tmp`, verifies size, renames; a leftover temp file after a power cut is recovered; a corrupt file is **never overwritten at boot**; one-time migration from the old EEPROM struct; 3-second button at boot resets only the network section.

**Latent bug fixed on the way:** `chatMinMs`, `chatMaxMs`, `cooldownMs` were `uint16_t` while the CLI allowed 600 s (600 000 ms): silent wrap. Now 32-bit.

### 4.7 The ArduinoJson aliasing bug (found by a host test)

ArduinoJson 7 keeps a `const char*` **by pointer**. `toDoc()` therefore pointed the document at the settings' own string buffers; `fromDoc()` then `memset` a rule's buffer while reading it, so the rule read as empty and the whole change was rejected: with any rule defined, every `config set` failed. Fix: copy every string into the document. This is why the config layer has a host test built from the real `Settings.cpp` with a mock `String`, in-memory LittleFS and EEPROM.

### 4.8 Looks: presets sampled from photographs

The dashboard draws one parameterised SVG face (body, belly, ears, faceplate, pattern, mane tuft, optional hat, iris). Presets: **61**. Series 1-4 and Santa were sampled from a high-resolution photo of real Furbies by segmenting each Furby and taking the median colour of fixed regions (flanks, belly, ear corners, top of head), then rendered next to the photo to check. Sampling is fooled by patterns (zebra stripes, tiger, snow leopard, cheetah, bumble bee) so those were corrected by eye. Series 5-10 come from a 60-pixel-wide chart, so fidelity is lower and a few crops landed on the wrong cell (champagne, lion are estimates). Naming follows the author's photo and the Official Furby Wiki for series 1-4; the chart's series 2/3 swap two names, kept as the photo has them so existing presets stay valid. Colours are **approximations**, stated everywhere.

### 4.9 Sensors, network and OSC

**Sensors.** `pir` and `hcsr04`, because they need no library. Presence = median of 3, hysteresis (`wake_mm`/`sleep_mm`), hold time (`smooth_ms`); pure logic in `SensorLogic.h`, host-tested including clock wrap. HC-SR04 echo is timed in an interrupt so `loop()` never blocks. Two hardware traps recorded: the classic echo line is **5 V** (Pico is not 5 V tolerant), and **GP4/GP5 are Furby channels 5 and 6**, so I2C goes on I2C1 (GP26/GP27). A ToF sensor needs a library and a decision about which one.

**Network.** Decision (the author's): a **dedicated router**, fixed addresses, every hub a peer, rather than "hub 1 is the access point" (a single point of failure and a handful-of-clients limit). The hub's own AP stays as fallback and recovery.

**Router join cannot be asynchronous** (see the table in section 3), which corrected an earlier claim in this project that it would never block. Final design: first join in `setup()` with 15 s; runtime retries with a 4 s timeout, back-off 10/20/30/60 s, watchdog widened to 20 s around the call. Worst case with the router absent: the main loop stalls about 4 s per minute; **core 1 is unaffected**, sensing and Furbies keep working. If that stall matters, the fix is the SDK's async API (`cyw43_arch_wifi_connect_async`) bypassing arduino-pico's WiFi class, which is entangled with the netif; not attempted without hardware.

**OSC.** Hand-written 100-line codec (i, f, s), verified against the OSC 1.0 spec byte examples and against hostile input. Each hub **broadcasts** presence, distance and a 2 s status heartbeat, and accepts a **whitelist** of commands (`wake sleep send wakeall sleepall trigger stop`) mapped one-to-one onto CLI commands. Reactions are `rules` in the JSON (`on` pattern, `do` = any CLI line, delay), evaluated against *other* hubs' events with edge detection, at most one firing per 300 ms per rule. No central brain. No authentication.

### 4.10 Power analysis (unresolved)

From the schematic text only: an MP8759 buck, VIN 6-24 V, output the servo rail and Pico VSYS (limit 5.5 V). So 6 V in gives about 5 V on the pins and the rail must not be raised. Options in README. **Nothing here was measured.** The first bench action is to try a Furby at 5 V.

### 4.11 Process lessons

- Patch scripts that fail silently look like successes: use `assert old in text` before every replace.
- A "success" from a cached build proves nothing; rebuild after the actual edit.
- Deliver files with their folder paths; cards that show only a name led to test files landing in `src/`.
- Read the framework source before relying on an API's name (`beginNoBlock`).

## 5. Architecture in detail

```
core 0  loop():  watchdog reset > Cli.poll > Sensor.loop > Standalone.loop > Dashboard.loop > Osc.loop > LED
core 1  loop1(): FurbyBus.coreTick()  (blocks until the next 1050 us tick)
```

| Module | Responsibility | Notes |
|---|---|---|
| `FurbyBus` | per-Furby action FIFOs (depth 8), tick engine, GPIO drive/float | shared status is `volatile`; commands arrive via `queue_t` (64) |
| `FurbyProtocol.h` | `bitValue()`, `pulseInSlot()` | pure, host-tested against the sketch |
| `Standalone` | state machine, stagger, chatter, button, 3 s cooldown logic | uses `Sensor.present()` |
| `Sensor` | PIR / HC-SR04, presence | type change needs reboot |
| `Settings` | JSON <-> struct, validation, paths, atomic save, migration | `Cfg.v` is the live struct; `Cfg.applyRuntime()` pushes timing to the engine |
| `Dashboard` | AP or router join, raw `WiFiServer`, DNS captive portal, mDNS, `/api/*` | one request per connection, closed immediately |
| `Cli` | the only command language (serial, web, OSC, rules) | scripts (`sweep`, `selftest`) run from `poll()` |
| `Osc` + `OscCodec` + `RuleMatch` | UDP, peers, rules | active only in router mode or with an explicit `osc_target` |
| `Diag` | live log, breadcrumbs, switches | EEPROM bytes at offset 160 hold the switches |

**Core 1 rules.** Core 1 must not call WiFi, Serial, `String` or allocate. It touches only GPIO, `time_us_64()` and its own state. Everything else enqueues.

**Timing constants.** Pulse 150 us; half-bit 1050 us; gap 90 ms (86 ticks); wake 100 ms then 50 ms settle; sleep = message, 800 ms, message; message = 6 repeats; 18 slots per packet; 14 pulses per packet.

**Memory.** Engine state is static; ArduinoJson documents are built per request and freed (a `JsonDocument` of the whole config is a few KB).

## 6. Interfaces

**HTTP** (one request per connection): `GET /` page; `GET /api/status`; `GET /api/cli?line=<command>` plain text; `GET /api/config` redacted JSON; `POST /api/config` body is JSON (partial documents merge; at most 8 KB), reply `{"ok":true,"saved":bool,"reboot":bool}` or 400 `{"ok":false,"error":"..."}`; `/favicon.ico` returns 204; anything else redirects to `/` (captive portal probes).

**`/api/status` fields:** `unit, ssid, fw, auto, state, remaining, sensor, stype, dist, net, want, link, mask, f, peers, stations, script`. `f` is 16 arrays `[assumed(0 unknown, 1 asleep, 2 awake), lastCode, busy, ageSeconds]`; `peers` arrays `[id, presence, distMm, ageSeconds]`.

**OSC, config schema, CLI grammar:** see README (kept there to have one copy).

## 7. Invariants and gotchas (keep these true)

1. Compare times with a **signed difference** and a **fresh** `millis()`; never subtract a stale snapshot.
2. Never gate a send on `connected()`; never call `softAPgetStationNum()` outside `refreshStations()`.
3. Every string put into an ArduinoJson document must be copied.
4. `WiFi.config()` before every `WiFi.begin*()`; widen the watchdog around joins; never join in a tight loop.
5. Core 1 stays free of WiFi, Serial, heap and flash.
6. Flash writes stall both cores: save only on explicit request.
7. Configuration changes are all-or-nothing; a corrupt config is never overwritten at boot.
8. Furbies ignore packets while busy; chatter and sleep logic account for that. Assumed state is not a measurement.
9. Furby numbers are 1-based in UI, CLI, API and JSON paths; rule indexes are 0-based.
10. A watchdog or auto-restart must log why it fired.

## 8. Testing strategy

Everything that can be proven off-hardware is, because the development environment had no board. `test/run_host_tests.sh` builds five programs with plain `g++`; mocks live in `test/mock/` (engine: clock, GPIO, queue) and `test/mock_cfg/` (String, IPAddress, LittleFS, EEPROM). The real `FurbyBus.cpp` and `Settings.cpp` are compiled unmodified. To add a test, put pure logic in a header (see `SensorLogic.h`, `RuleMatch.h`, `OscCodec.h`) so it needs no mocks. Not coverable: the radio, real timing on silicon, UDP broadcast through the router, the Furbies, a browser.

## 9. Bring-up checklist (next hardware session)

1. `status`, then `reboot`, `status`: expect `last reset: commanded reboot`, `config: loaded furbyhub.json`.
2. One Furby: `hold 1 float` and measure idle voltage; `wake 1`; `send 1 name`; `sleep 1`; `sweep 1 20 200 20`.
3. Dashboard on a phone; `debug on` and confirm `200, N of N bytes sent` and no `AP: restarting`. Change a look, reboot, confirm it persists.
4. PIR: `sensor`; walk past; `sensor: PRESENT`; auto mode wakes, 60 s later sleeps.
5. HC-SR04 (3.3 V echo!): `config set sensor.type hcsr04`, `save`, `reboot`, `sensor watch 30`, tune `wake_mm`/`sleep_mm`.
6. Router: `config set network.ssid ...`, `network.pass`, `network.ip`, `network.mode sta`, `save`, `reboot`; `net` shows CONNECTED, IP and dBm.
7. Two hubs: `osc status` lists the other within seconds; `rule add hub/2/presence 400 wakeall`, `save`; walk up to hub 2.
8. Power cut test: boot hubs before the router; confirm they retry and join, and that sensing worked meanwhile.
9. Check UDP broadcast between hubs; if empty, look for router "AP isolation".
10. Measure the Furby supply voltage at 16 awake Furbies.

## 10. Open questions and backlog

| Priority | Item |
|---|---|
| High | Verify steps 4-8 above on hardware; update the README status table |
| High | Confirm the 3.3 V signal on all 16 Furbies, not only one; decide on the 74HCT245 buffer |
| High | Resolve the 6 V supply route by measuring the board |
| Medium | Truly asynchronous router join via the SDK async API (removes the ~4 s stall) |
| Medium | Time-of-flight sensor support (VL53L1X) once the author picks one |
| Medium | Confirm the packet structure independently; map the other ten packets |
| Medium | Photos of series 5-10 Furbies for properly sampled presets |
| Low | Authentication for the dashboard, `/api/cli` and OSC commands |
| Low | Verify mDNS (`furbyhub<n>.local`) |
| Low | Maximum AP clients on a hub's own WiFi (not checked) |
| Low | Choose a licence; update `docs/`; add a `.gitignore` for `.pio/` |

## 11. File inventory

| File | Lines | Role |
|---|---|---|
| `src/main.cpp` | 88 | two-core entry points, boot recovery, LED |
| `src/FurbyBus.cpp`, `include/FurbyBus.h` | 281, 83 | pulse engine |
| `include/FurbyProtocol.h`, `FurbyCodes.h` | 43, 30 | packet logic, named codes |
| `src/Standalone.cpp`, `include/Standalone.h` | 147, 59 | standalone behaviour |
| `src/Sensor.cpp`, `include/Sensor.h`, `SensorLogic.h` | 79, 38, 40 | sensors |
| `src/Settings.cpp`, `include/Settings.h` | 395, 103 | configuration |
| `src/Dashboard.cpp`, `include/Dashboard.h` | 466, 47 | network, HTTP |
| `include/DashboardPage.h` | 485 | the entire web UI |
| `src/Cli.cpp`, `include/Cli.h` | 598, 45 | command language |
| `src/Osc.cpp`, `include/Osc.h`, `OscCodec.h`, `RuleMatch.h` | 224, 70, 104, 41 | OSC and rules |
| `src/Diag.cpp`, `include/Diag.h` | 82, 52 | diagnostics |
| `include/config.h` | 34 | pins, AP address, watchdog |
| `test/*` | about 560 | host tests and mocks |

## 12. Glossary

| Term | Meaning |
|---|---|
| hub | one Pico W with its Waveshare board and up to 16 Furbies |
| tick | one half-bit time, 1050 us; the engine's step |
| packet / message | 9-bit frame / the packet repeated 6 times |
| assumed state | what the hub last told a Furby; Furbies cannot answer |
| STA / AP | joining a router / being an access point |
| breadcrumb | a code-stage number kept across a watchdog reset |
| preset | a fur pattern drawing in the dashboard |

## 13. Regenerating the illustrations

`docs/img/*.svg` are plain SVG. `docs/tools/make_docs.py` regenerates the hero, dashboard illustration, architecture, network, wiring, protocol and state-machine diagrams from `final.json` (the 61 presets) and `facelib.py` (the face template), writing into `docs/img/`. The fur chart `furby-looks.svg` was produced by a separate one-off script that is **not** included; regenerate it from the same two files if presets change. When `PRESETS` in `DashboardPage.h` changes, update `final.json` to match.
