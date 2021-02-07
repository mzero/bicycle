#include "args.h"

#include "CLI11.hpp"

namespace Args {
  std::string configFilePath = "bicycle.config";
  std::string logFilePath = "_bicycle_log";
  bool configCheckOnly = false;

  bool sendMidiClock = false;

  Meter meter;

  int exitCode = 0;


  bool parse(int argc, char* argv[]) {
    CLI::App app{"bicycle - a MIDI looper"};

    app.add_option("-c,--config-file", configFilePath, "defaults to " + configFilePath);
    app.add_option("-l,--log-file", logFilePath, "defaults to " + logFilePath);
    app.add_flag("-C,--check-config", configCheckOnly, "exit after config parse");

    app.add_flag("-s,--sync-out", sendMidiClock, "send MIDI clock sync");

    app.add_flag("-b,--beats", meter.beats, "fix beats in first layer")
      ->check(CLI::Range(1,16));
    app.add_flag("-p,--pulse", meter.base, "pulse of fixed meter, 4, 8, etc..")
      ->check(CLI::IsMember({1, 2, 4, 8, 16}));

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exitCode = app.exit(e);
        return false;
    }

    return true;
  }
}
