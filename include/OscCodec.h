#pragma once
// Minimal OSC 1.0 message encoder/decoder (no bundles). Pure functions, host-tested
// (test/osc_check.cpp) against the byte examples of the OSC specification.
// Supported argument types: i (int32), f (float32), s (string); T/F decode as 1/0.
#include <stdint.h>
#include <string.h>

namespace OscCodec {

constexpr size_t MAX_ARGS = 6, MAX_STR = 40, MAX_ADDR = 64;

struct Arg {
  char    type = 'i';
  int32_t i = 0;
  float   f = 0;
  char    s[MAX_STR] = {0};
};
inline Arg argI(int32_t v)      { Arg a; a.type = 'i'; a.i = v; return a; }
inline Arg argF(float v)        { Arg a; a.type = 'f'; a.f = v; return a; }
inline Arg argS(const char* v)  { Arg a; a.type = 's'; strncpy(a.s, v, MAX_STR - 1); return a; }

struct Msg {
  char    addr[MAX_ADDR] = {0};
  uint8_t n = 0;
  Arg     a[MAX_ARGS];
};

inline size_t padded(size_t len) { return (len + 4) & ~(size_t)3; }   // string + NUL, rounded up to 4

inline size_t putStr(uint8_t* b, size_t cap, size_t pos, const char* s) {
  const size_t len = strlen(s), pl = padded(len);
  if (pos + pl > cap) return 0;
  memcpy(b + pos, s, len);
  memset(b + pos + len, 0, pl - len);
  return pos + pl;
}
inline size_t put32(uint8_t* b, size_t cap, size_t pos, uint32_t v) {
  if (pos + 4 > cap) return 0;
  b[pos] = v >> 24; b[pos + 1] = v >> 16; b[pos + 2] = v >> 8; b[pos + 3] = v;
  return pos + 4;
}

// returns the packet length, 0 if it does not fit
inline size_t encode(uint8_t* b, size_t cap, const char* addr, const Arg* args, uint8_t n) {
  size_t pos = putStr(b, cap, 0, addr);
  if (!pos) return 0;
  char tag[MAX_ARGS + 2] = ",";
  for (uint8_t k = 0; k < n && k < MAX_ARGS; k++) tag[k + 1] = args[k].type;
  tag[(n < MAX_ARGS ? n : MAX_ARGS) + 1] = 0;
  pos = putStr(b, cap, pos, tag);
  if (!pos) return 0;
  for (uint8_t k = 0; k < n && k < MAX_ARGS; k++) {
    if (args[k].type == 'i')      pos = put32(b, cap, pos, (uint32_t)args[k].i);
    else if (args[k].type == 'f') { uint32_t u; memcpy(&u, &args[k].f, 4); pos = put32(b, cap, pos, u); }
    else                          pos = putStr(b, cap, pos, args[k].s);
    if (!pos) return 0;
  }
  return pos;
}

inline bool getStr(const uint8_t* b, size_t len, size_t& pos, char* out, size_t cap) {
  size_t i = pos;
  while (i < len && b[i]) i++;
  if (i >= len) return false;                      // no terminator
  const size_t n = i - pos;
  if (n >= cap) return false;
  memcpy(out, b + pos, n);
  out[n] = 0;
  pos += padded(n);
  return pos <= len;
}

inline bool decode(const uint8_t* b, size_t len, Msg& m) {
  m = Msg();
  if (len < 4 || b[0] != '/') return false;        // also rejects "#bundle"
  size_t pos = 0;
  if (!getStr(b, len, pos, m.addr, MAX_ADDR)) return false;
  if (pos >= len) return true;                     // very old senders: no type tag
  char tag[16];
  if (!getStr(b, len, pos, tag, sizeof tag) || tag[0] != ',') return false;
  for (const char* t = tag + 1; *t; t++) {
    if (m.n >= MAX_ARGS) return false;
    Arg& a = m.a[m.n];
    if (*t == 'i') {
      if (pos + 4 > len) return false;
      a.type = 'i'; a.i = (int32_t)((uint32_t)b[pos] << 24 | b[pos + 1] << 16 | b[pos + 2] << 8 | b[pos + 3]); pos += 4;
    } else if (*t == 'f') {
      if (pos + 4 > len) return false;
      const uint32_t u = (uint32_t)b[pos] << 24 | b[pos + 1] << 16 | b[pos + 2] << 8 | b[pos + 3];
      a.type = 'f'; memcpy(&a.f, &u, 4); pos += 4;
    } else if (*t == 's') {
      a.type = 's';
      if (!getStr(b, len, pos, a.s, MAX_STR)) return false;
    } else if (*t == 'T' || *t == 'F') {
      a.type = 'i'; a.i = (*t == 'T');
    } else {
      return false;                                // unsupported type: ignore the whole message
    }
    m.n++;
  }
  return true;
}

}  // namespace OscCodec
