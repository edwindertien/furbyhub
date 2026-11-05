#pragma once
// ---------------------------------------------------------------------------
// Original (1999) Furby wired/IR protocol - pure functions, no hardware.
//
// Kept dependency-free so it can be unit-tested on a PC
// (see test/protocol_check.cpp).
//
// As implemented in the original FurbyCentralMegaADK sketch:
//   * a packet is 9 bits: start bit (always 1), 4 data bits (LSB first),
//     then the same 4 bits inverted (LSB first)
//   * every bit takes one "bit time" (~2.1 ms) = two half-bit slots
//   * there is always a pulse in the first half-slot; a logic 1 has a second
//     pulse in the second half-slot, a logic 0 has none
//   * the packet is repeated several times (6) with a gap of ~90 ms between
//
// NOTE: the 4-bit / 9-bit structure is taken from the working sketch and the
// notes in its header; it has not been independently confirmed.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace FurbyProtocol {

constexpr uint8_t BITS_PER_PACKET  = 9;
constexpr uint8_t SLOTS_PER_PACKET = BITS_PER_PACKET * 2;   // 2 half-bit slots per bit

// Logical value of bit k (0..8) of the packet for a 4-bit code.
inline bool bitValue(uint8_t code, uint8_t k) {
  if (k == 0) return true;                       // start bit
  if (k <= 4) return (code >> (k - 1)) & 1;      // data bits, LSB first
  return !((code >> (k - 5)) & 1);               // inverted data bits
}

// Does slot s (0..17) of the packet carry a pulse?
inline bool pulseInSlot(uint8_t code, uint8_t slot) {
  const uint8_t bit    = slot >> 1;
  const bool    second = slot & 1;
  return !second || bitValue(code, bit);
}

// The documented a..p letter notation: a = 0 ... p = 15
inline char letterFor(uint8_t code) { return 'a' + (code & 0x0F); }

}  // namespace FurbyProtocol
