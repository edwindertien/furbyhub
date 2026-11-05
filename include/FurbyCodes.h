#pragma once
// Named packet codes. Only the ones documented in the original sketch are named;
// the other packets exist (16 in total, a..p) but their meaning is personality
// dependent and is left numeric on purpose.
#include <stdint.h>

struct CodeInfo {
  const char* key;     // CLI keyword
  uint8_t     code;
  const char* label;   // human readable
};

constexpr CodeInfo CODE_TABLE[] = {
  { "name",   0x00, "Say my name"        },
  { "dance",  0x05, "Dance / party"      },
  { "sneeze", 0x08, "Sneeze"             },
  { "sheep",  0x0B, "Ba ba black sheep"  },
  { "bed",    0x0E, "Go to bed"          },
  { "sleep",  0x0F, "Sleep"              },
};
constexpr uint8_t CODE_TABLE_LEN = sizeof(CODE_TABLE) / sizeof(CODE_TABLE[0]);

constexpr uint8_t CODE_NAME  = 0x00;
constexpr uint8_t CODE_SLEEP = 0x0F;
constexpr uint8_t CODE_BED   = 0x0E;

// Codes the standalone mode picks from while the furbies are awake
// (all of them safe: none puts a furby to sleep)
constexpr uint8_t CHATTER_CODES[] = { 0x00, 0x05, 0x08, 0x0B };
constexpr uint8_t CHATTER_CODES_LEN = sizeof(CHATTER_CODES);
