#!/bin/sh
# Host-side tests: no hardware, no PlatformIO needed. Run from the test/ directory.
set -e
cd "$(dirname "$0")"
echo "== protocol vs original sketch =="
g++ -std=c++17 -Wall -I ../include protocol_check.cpp -o /tmp/protocol_check
/tmp/protocol_check
echo
echo "== FurbyBus engine simulation (real FurbyBus.cpp, fake GPIO + clock) =="
g++ -std=c++17 -Wall -Wno-unused-function -I mock -I ../include engine_sim.cpp ../src/FurbyBus.cpp -o /tmp/engine_sim
/tmp/engine_sim
