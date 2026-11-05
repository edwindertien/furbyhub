#pragma once
#include <Arduino.h>

#define FW_NAME    "reFurbyshment"
#define FW_VERSION "0.1.0"

// ── Furby channels ──────────────────────────────────────────────────────────
// Furby n (1-based everywhere in the UI / CLI / API) is wired to FURBY_PIN[n-1].
//
// On the Waveshare Pico-Servo-Driver the four 4-pin signal headers P2..P5 carry
// GP0..GP15 (taken from the schematic), so furby 1 = first servo port = GP0.
// If your silkscreen numbering differs, change the table - nothing else cares.
constexpr uint8_t NUM_FURBIES = 16;
constexpr uint8_t FURBY_PIN[NUM_FURBIES] = {
   0,  1,  2,  3,  4,  5,  6,  7,
   8,  9, 10, 11, 12, 13, 14, 15
};

// ── Standalone inputs (free pins on the Pico, broken out on the board's H1) ─
constexpr uint8_t PIN_SENSOR         = 22;     // PIR / presence sensor output
constexpr bool    SENSOR_ACTIVE_HIGH = true;
constexpr uint8_t PIN_BUTTON         = 21;     // momentary button to GND (wake / sleep toggle)
constexpr uint16_t INPUT_DEBOUNCE_MS = 60;

// ── WiFi access point ───────────────────────────────────────────────────────
// SSID is "reFurby-<unit>", e.g. reFurby-3. All five units use the same IP; they
// are separate networks. Units are spread over channels 1 / 6 / 11.
#define WIFI_SSID_PREFIX "reFurby-"
#define WIFI_PASS        "furbyfurby"          // >= 8 characters for WPA2
#define AP_IP_OCTETS     192, 168, 42, 1

// ── Safety ──────────────────────────────────────────────────────────────────
constexpr uint32_t WATCHDOG_MS = 8000;         // core 0 is reset if it stops looping
