#include "args.h"

#include "CLI11.hpp"

namespace Args {
  std::string configFilePath = "bicycle.config";
  std::string logFilePath = "_bicycle_log";
  bool configCheckOnly = false;

  bool sendMidiClock = false;
  bool receiveMidiClock = false;

  bool tempoSet = false;
  double tempoBPM = 0.0;

  bool tempoRangeSet = false;
  double tempoLowBPM = 0.0;
  double tempoHighBPM = 0.0;

  bool meterSet = false;
  int meterBeats;
  int meterBase;

  int exitCode = 0;

  void setTempo(double t) {
    tempoSet = true;
    tempoBPM = t;
  }

  using tempo_range_t = std::vector<double>;

  void setTempoRange(const tempo_range_t& vs) {
    tempoRangeSet = true;

    switch (vs.size()) {
      case 2:
        tempoLowBPM = std::min(vs[0], vs[1]);
        tempoHighBPM = std::max(vs[0], vs[1]);
        break;

      default:
        throw CLI::ValidationError("tempo range should two BPM values, like: 75-140");
    }
  };

  using meter_arg_t = std::vector<unsigned int>;

  void setMeter(const meter_arg_t& vs) {
    meterSet = true;

    switch (vs.size()) {
      case 1:   meterBeats = vs[0]; meterBase = 4;      break;
      case 2:   meterBeats = vs[0]; meterBase = vs[1];  break;
      default:
        throw CLI::ValidationError("meters should formatted a 3/4, or just a number of beats");
    }
  };

  bool parse(int argc, char* argv[]) {
    CLI::App app{"bicycle - a MIDI looper"};

    // app.add_option("-c,--config-file", configFilePath, "defaults to " + configFilePath);
    // app.add_option("-l,--log-file", logFilePath, "defaults to " + logFilePath);
    app.add_option("-c,--config-file", configFilePath, "")
      ->capture_default_str();
    app.add_option("-l,--log-file", logFilePath, "")
      ->capture_default_str();

    app.add_flag("-C,--check-config", configCheckOnly, "exit after config parse");

    app.add_flag("-s,--sync-out", sendMidiClock, "send MIDI clock sync");
    app.add_flag("-S,--sync-in", receiveMidiClock, "receive MIDI clock sync");

    app.add_option_function<double>("-t,--tempo", setTempo, "set a fixed tempo");
    app.add_option_function<tempo_range_t>("-r,--range", setTempoRange, "set tempo range")
      ->delimiter('-')
      ->type_name("TEMPOS");

    app.add_option_function<meter_arg_t>("-m,--meter", setMeter, "set a fixed meter")
      ->delimiter('/')
      ->type_name("METER");


    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exitCode = app.exit(e);
        return false;
    }

    return true;
  }
}
