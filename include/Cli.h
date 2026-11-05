#pragma once
#include <Arduino.h>
#include <vector>

// Collects Print output into a String (used to answer web requests with CLI output).
class StringPrint : public Print {
public:
  String s;
  size_t write(uint8_t c) override { s += (char)c; return 1; }
  size_t write(const uint8_t* b, size_t n) override { s.concat((const char*)b, (unsigned)n); return n; }
};

// ---------------------------------------------------------------------------
// One command language for everything: typed over USB serial, or sent by the
// dashboard (/api/cli?line=...). If it works in the web UI it works on the
// bench, and vice versa.
// ---------------------------------------------------------------------------
class CliClass {
public:
  void begin();
  void poll();                                        // serial input + running test scripts
  void execute(const String& line, Print& out);

  bool   scriptRunning() const { return _scriptRunning; }
  const String& log() const { return _log; }          // output of the last/running test script

private:
  struct Step { uint32_t afterMs; String line; };

  void startScript(std::vector<Step>& steps, Print& out);
  void stopScript();
  void applyTiming();
  void printStatus(Print& out);
  void printHelp(Print& out);

  String _rx;
  std::vector<Step> _script;
  size_t   _scriptPos = 0;
  uint32_t _scriptDue = 0;
  bool     _scriptRunning = false;
  String   _log;
};

extern CliClass Cli;
