#include "args.h"

#include "CLI11.hpp"

namespace Args {
  std::string configFilePath = "bicycle.config";
  std::string logFilePath = "_bicycle_log";
  bool configCheckOnly = false;

  int exitCode = 0;

  bool parse(int argc, char* argv[]) {
    CLI::App app{"bicycle - a MIDI looper"};

    app.add_option("-c,--config-file", configFilePath, "defaults to " + configFilePath);
    app.add_option("-l,--log-file", logFilePath, "defaults to " + logFilePath);
    app.add_flag("-C,--check-config", configCheckOnly, "exit after config parse");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exitCode = app.exit(e);
        return false;
    }

    return true;
  }
}
