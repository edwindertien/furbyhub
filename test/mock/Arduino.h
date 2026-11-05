// Minimal host stand-in for the pieces of Arduino/pico-sdk that FurbyBus.cpp uses.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#define PROGMEM
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
extern uint64_t g_sim_us;
inline uint32_t millis() { return (uint32_t)(g_sim_us / 1000); }
inline uint32_t micros() { return (uint32_t)g_sim_us; }
