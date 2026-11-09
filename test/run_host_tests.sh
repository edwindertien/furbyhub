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
echo
echo "== presence logic =="
g++ -std=c++17 -Wall -I ../include sensor_check.cpp -o /tmp/sensor_check
/tmp/sensor_check
echo
echo "== OSC codec + rule matching =="
g++ -std=c++17 -Wall -Wextra -I ../include osc_rule_check.cpp -o /tmp/osc_rule_check
/tmp/osc_rule_check
echo
echo "== config layer: real Settings.cpp + ArduinoJson, in-memory LittleFS/EEPROM =="
AJ=../.pio/libdeps/picow/ArduinoJson/src
if [ -d "$AJ" ]; then
  g++ -std=c++17 -Wall -Wno-unused-function -DHAVE_STRLCPY -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -I mock_cfg -I $AJ -I ../include settings_check.cpp ../src/Settings.cpp -o /tmp/settings_check
  /tmp/settings_check
else
  echo "(skipped: run 'pio run' once so ArduinoJson is downloaded to .pio/libdeps)"
fi
