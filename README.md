# reFurbyshment

Distributed controller for the reFurbyshment project, a retirement home for 1999-style Furbies.
Five Pico W units, each running 16 furbies over standard 3-wire servo leads, replace the single
central Arduino Mega. Every unit works standalone (sensor / button) and has its own WiFi dashboard.

> **Status: not yet tested on hardware.** Firmware compiles cleanly, the pulse engine is verified
> against the original Mega sketch in a host simulation (see *Tests*). The WiFi dashboard, the
> captive portal and the standalone logic have not been run on a real Pico W yet.

## Hardware

| Part | Details |
|---|---|
| MCU | Raspberry Pi Pico W |
| Carrier | Waveshare Pico-Servo-Driver (16 servo ports) |
| Furby n | GPIO n-1: furby 1 = GP0 ... furby 16 = GP15 (schematic: P2..P5 carry GP0..GP15) |
| Presence sensor | GP22 (active high, pulled down so an unplugged sensor reads idle) |
| Button | GP21 to GND: wake when idle, sleep when awake |

Verify that furby 1 really is GP0 on your silkscreen, using `hold 1 high` and a multimeter. If it isn't,
edit `FURBY_PIN[]` in `include/config.h`; nothing else depends on the mapping.

### Wire inside the furby (from the original sketch)

Signal wire, through a 10 k series resistor each, to the IR receiver input (6-pin connector pin 1) and
the tilt sensor input (4-pin connector pin 1). Red and brown to the battery terminals.

- **Long HIGH pulse, then floating**: acts like the tilt sensor, wakes the furby.
- **Short pulses**: the IR protocol (below).
- The unit leaves every wire floating (high-Z) unless it is actively talking.

### Power: read before connecting 6 V

The board's servo rail is the output of its on-board 5 V buck regulator, not the supply terminal, and the
Pico's VSYS shares that rail. Feeding 6 V into the VIN terminal therefore gives about 5 V on the servo pins.
Do **not** put 6 V directly on the rail pins while the regulator / Pico are connected to it.

1. Try one furby on the 5 V rail first. It may be perfectly happy.
2. If it needs 6 V: separate 6 V supply and bus, **common ground** with the board, and use the board
   for signal and ground only (de-pin the red wire from each servo plug).

Check continuity with a multimeter before relying on either; this was derived from the schematic netlist, not the board.
Sixteen furbies on the regulator's 8 A rating is the reason wake-up is staggered (`wakestagger`, default 300 ms).

### 3.3 V logic

The Pico drives 3.3 V where the Mega drove 5 V. Whether the furby inputs accept that is what the bench commands are for.
Before connecting, measure the idle voltage on the signal wire with the furby powered: the RP2040 is not 5 V tolerant
(the 10 k series resistors limit the current but check anyway). If 3.3 V is marginal, put a 74HCT245 buffer on 5 V between
Pico and furby.

## Protocol (as implemented)

From the working Mega sketch; the structure has **not** been independently confirmed.

- Packet = 9 bits: start bit (1), 4 data bits LSB first, the same 4 bits inverted.
- Each bit = 2 half-slots (default 1050 us each). First half always has a pulse, a 1 has a second pulse in the second half.
- Pulse 150 us. Packet repeated 6 times, about 109 ms apart. Wire floats in the gaps.
- 16 codes, a..p = 0..15. Named from the sketch: 0 name, 5 dance, 8 sneeze, 11 ba-ba-black-sheep, 14 bed, 15 sleep.
- Furbies ignore packets while busy; sleep is therefore sent twice, 800 ms apart, like the original.

## Build and flash

```bash
pio run                      # build
pio run -t upload            # flash (hold BOOTSEL while plugging in the first time)
pio device monitor           # CLI over USB serial, 115200
```

One binary for all units. Give each its number once: `unit 3`, `reboot`. SSID becomes `reFurby-3`
(channels 1 / 6 / 11 by unit number). Password `furbyfurby`, set in `include/config.h`.
Dashboard: `http://192.168.42.1` (the captive portal opens it by itself on most phones).

## Dashboard

Tap furby tiles to select them, then wake, sleep or send a message. "Wake everyone / Everyone to sleep" are staggered.
"Run by itself" toggles the standalone behaviour. Furbies cannot answer over this wire, so the eyes show what
the unit last told each furby, not a measurement. The console box accepts every CLI command.

## Standalone behaviour

`idle -> waking -> awake -> sleeping -> idle`. While **auto** is on: the sensor wakes everybody, they stay awake while
somebody is still detected plus `awake` seconds (default 60, as in the original), may chat at random, then go to sleep.
With auto off nothing happens by itself; manual commands and the button always work.

## CLI

`<set>` = `all` | `3` | `1,4,7` | `2-5` (1-based; `all` = enabled furbies). `<code>` = `a..p`, `0..15`, `name dance sneeze sheep bed sleep`.

| Command | What it does |
|---|---|
| `wake <set>` / `sleep <set>` | long pulse / sleep message sent twice |
| `send <set> <code> [reps]` | one message |
| `wakeall` / `sleepall` / `trigger` | staggered group actions, same as the dashboard / sensor |
| `pulse <set> <ms>` | HIGH for ms, then float |
| `hold <set> high\|low\|float [ms]` | drive the wire for measuring |
| `sweep <n> [from [to [step]]]` | wake furby n, send "name" at pulse widths from..to us |
| `selftest <set>` | wake, name, dance, sleep |
| `stop`, `log` | abort everything / show last script output |
| `status`, `sensor`, `pins`, `help` | information |
| `enable <set>` / `disable <set>` | which furbies are connected (saved) |
| `auto on\|off`, `unit <n>` | saved immediately |
| `timing [pulse\|bit\|half\|repeats\|gap\|wake <v>]` | RAM only until `save` |
| `set awake\|chatmin\|chatmax\|cooldown <s>`, `set wakestagger\|sleepstagger <ms>`, `set chatter on\|off` | RAM only until `save` |
| `save`, `defaults`, `reboot` | |

### Testing the 3.3 V hack

1. `status`, then `hold 1 float`: wire floating. Measure idle voltage at the furby side.
2. `pulse 1 100`: does the furby wake?
3. `send 1 name`: does it say its name?
4. `sweep 1 20 200 20`: note the pulse widths that work (the sketch says 8-208 us has been seen to work).
5. Try `timing repeats 12` or `timing half 1500` if reception is flaky, then `save`.

## HTTP API

- `GET /` dashboard
- `GET /api/status` JSON: unit, mode, `f[n] = [assumed(0 unknown/1 asleep/2 awake), lastCode, busy, ageSeconds]`
- `GET /api/cli?line=<command>` runs a CLI command, plain-text reply

Anyone on the WPA2 network can run any CLI command, including `reboot` and `defaults`.

## Architecture

- **core 1** (`FurbyBus`): one tick per half-bit; every furby is stepped each tick, so 16 cost the same as 1.
  Core 0 sends small actions (Wake / Msg / Wait / Level) through a queue; per-furby FIFO of 8.
  After a wake pulse the wire is forced to float for 50 ms.
- **core 0**: WiFi AP + DNS + `WebServer`, CLI, standalone state machine. A watchdog (8 s) resets core 0 if it hangs.
  If nobody has connected for 10 minutes the AP is rebuilt.
- Core 1 is launched before `setup()` runs on core 0, hence the `ready()` handshake in `setup1()`.

```
platformio.ini
include/  config.h Settings.h FurbyBus.h FurbyProtocol.h FurbyCodes.h Standalone.h Cli.h Dashboard.h DashboardPage.h
src/      main.cpp FurbyBus.cpp Standalone.cpp Cli.cpp Dashboard.cpp Settings.cpp
test/     host tests (no hardware needed): ./run_host_tests.sh
```

## Tests

`cd test && ./run_host_tests.sh` compiles the real `FurbyBus.cpp` against a fake GPIO and clock and checks:
all 16 codes match the original sketch's pulse pattern; 150 us pulses; 109.2 ms repeat period (original 108.7 ms);
100 ms wake pulse; 16 simultaneous channels; sleep macro; `clear()`; float after wake.
It cannot test WiFi, timing on real silicon, or the furbies' reaction.

## Known open points

- Protocol structure unconfirmed outside the original sketch.
- 3.3 V compatibility and 5 V vs 6 V furby supply: bench questions.
- Chatter uses only codes 0, 5, 8, 11. The other packets' effects are unknown.
