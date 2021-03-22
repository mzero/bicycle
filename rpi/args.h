#ifndef _INCLUDE_ARGS_H_
#define _INCLUDE_ARGS_H_

#include <string>

// N.B.: It is important to keep this interface as minimal as possible
// as re-compiling args.cpp is very expensive.

namespace Args {
  extern std::string configFilePath;
  extern std::string logFilePath;
  extern bool configCheckOnly;

  extern bool sendMidiClock;
  extern bool receiveMidiClock;

  extern bool tempoSet;
  extern double tempoBPM;

  extern bool tempoRangeSet;
  extern double tempoLowBPM;
  extern double tempoHighBPM;

  extern bool meterSet;
  extern int meterBeats;  // 0 means unspecified
  extern int meterBase;

  extern int exitCode;
  bool parse(int argc, char* argv[]);
}

#endif // _INCLUDE_ARGS_H_
