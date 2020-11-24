#ifndef _INCLUDE_ARGS_H_
#define _INCLUDE_ARGS_H_

#include <string>

namespace Args {
  extern std::string configFilePath;
  extern std::string logFilePath;
  extern bool configCheckOnly;

  extern bool sendMidiClock;

  extern int exitCode;
  bool parse(int argc, char* argv[]);
}

#endif // _INCLUDE_ARGS_H_
