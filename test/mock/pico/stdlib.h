#pragma once
#include <stdint.h>
uint64_t time_us_64();
void busy_wait_us_32(uint32_t us);
void tight_loop_contents();
