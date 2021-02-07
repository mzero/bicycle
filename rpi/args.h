#ifndef _INCLUDE_ARGS_H_
#define _INCLUDE_ARGS_H_

#include <string>

#include "types.h"


namespace Args {
  extern std::string configFilePath;
  extern std::string logFilePath;
  extern bool configCheckOnly;

  extern bool sendMidiClock;

  extern Meter meter;

  extern int exitCode;
  bool parse(int argc, char* argv[]);
}

#endif // _INCLUDE_ARGS_H_
