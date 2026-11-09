#pragma once
// Does an event from another hub trigger a rule?   Pure logic, host-tested (test/rule_check.cpp).
//
//   rule "on" syntax:  hub/<N|*>/<what>[<op><value>]
//     hub/3/presence        hub 3 presence goes 0 -> 1
//     hub/*/presence=0      any hub's presence goes to 0
//     hub/2/distance<800    hub 2 distance drops below 800 mm (on the crossing, not while below)
//     hub/2/distance>1500   ... rises above 1500 mm
//
//   prev = value in the previous event from that hub, or -1 if this is the first one
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

inline bool ruleMatches(const char* on, uint8_t hubId, const char* what, int32_t prev, int32_t cur) {
  const char* p = on;
  if (strncmp(p, "hub/", 4) != 0) return false;
  p += 4;
  if (*p == '*') p++;
  else {
    int id = 0; bool any = false;
    while (*p >= '0' && *p <= '9') { id = id * 10 + (*p - '0'); p++; any = true; }
    if (!any || id != hubId) return false;
  }
  if (*p != '/') return false;
  p++;
  size_t wl = 0;
  while (p[wl] && p[wl] != '=' && p[wl] != '<' && p[wl] != '>') wl++;
  if (strlen(what) != wl || strncmp(p, what, wl) != 0) return false;
  p += wl;

  if (!*p) return strcmp(what, "presence") == 0 && cur == 1 && prev != 1;
  const char op = *p++;
  const int32_t val = atoi(p);
  switch (op) {
    case '=': return cur == val && prev != val;
    case '<': return cur >= 0 && cur < val && !(prev >= 0 && prev < val);
    case '>': return cur > val && !(prev > val);
  }
  return false;
}
