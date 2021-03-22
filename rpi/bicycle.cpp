#include <Arduino.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>

#include "args.h"
#include "configuration.h"
#include "displaythread.h"
#include "looper.h"
#include "message.h"
#include "midi.h"
#include "types.h"


Midi midi;
Loop theLoop;

void playEvent(const MidiEvent& ev) {
  midi.sendSynth(ev);
}

Tempo mapTempoCC(uint8_t v) {
  double t = 30.0 * std::exp(double(v)/55.0);
  return Tempo(t);
};

void noteEvent(const MidiEvent& ev) {

  Command cmd = Configuration::command(ev);
  switch (cmd.action) {
    case Action::none:        break;
    case Action::ignore:      return;
    case Action::arm:         if (ev.data2) theLoop.arm();        return;
    case Action::clear:       if (ev.data2) theLoop.clear();      return;
    case Action::keep:        if (ev.data2) theLoop.keep();       return;
    case Action::rearm:       if (ev.data2) theLoop.layerRearm(); return;
    case Action::good:        if (ev.data2) Message::good();      return;
    case Action::bad:         if (ev.data2) Message::bad();       return;

    case Action::layerArm:    if (ev.data2) theLoop.layerArm(cmd.layer);    return;
    case Action::layerMute:   theLoop.layerMute(cmd.layer, ev.data2 != 0);  return;
    case Action::layerVolume: theLoop.layerVolume(cmd.layer, ev.data2);     return;

    case Action::tempoLow: {
      TimingSpec ts = theLoop.getTimingSpec();
      ts.lowTempo = mapTempoCC(ev.data2);
      theLoop.setTimingSpec(ts);
      return;
    }
    case Action::tempoHigh: {
      TimingSpec ts = theLoop.getTimingSpec();
      ts.highTempo = mapTempoCC(ev.data2);
      theLoop.setTimingSpec(ts);
      return;
    }
    case Action::tempo: {
      TimingSpec ts = theLoop.getTimingSpec();
      switch (ev.data2) {
        case 0:   ts.tempoMode = TempoMode::inferred; break;
        case 127: ts.tempoMode = TempoMode::synced; break;
        default:  ts.tempoMode = TempoMode::locked;
                  ts.tempo = mapTempoCC(ev.data2);
                  theLoop.setTempo(ts.tempo);
      }
      theLoop.setTimingSpec(ts);
      return;
    }
    case Action::meterBase: {
      TimingSpec ts = theLoop.getTimingSpec();
      ts.meter.base = 1 << (ev.data2 / 26);
      theLoop.setTimingSpec(ts);
      return;
    }
    case Action::meterBeats: {
      TimingSpec ts = theLoop.getTimingSpec();
      ts.lockedMeter = ev.data2 != 0;
      ts.meter.beats = 1 + (ev.data2 / 8);
      theLoop.setTimingSpec(ts);
      return;
    }


  }

  switch (ev.status & 0xf0) {
    case 0x80: // Note Off
    case 0x90: // Note On
    case 0xa0: // Poly Aftertouch
    case 0xb0: // CC
      break;

    case 0xc0: // Program change
      return;     // TODO: echo these?

    case 0xd0: // Channel Aftertouch
    case 0xe0: // Pitch Bend
      break;

    case 0xf0: // System Messages
      return;

    default:
      return;
  }

  theLoop.addEvent(ev);
}

void updateMutes(const Loop::Status& s) {
  static bool firstTime = true;
  static bool lastMuted[Loop::Status::numLayers];

  for (int i = 0; i < s.layers.size(); ++i) {
    bool muted = s.layers[i].muted;
    if (!firstTime && lastMuted[i] == muted) continue;
    lastMuted[i] = muted;

    Command cmd(Action::layerMute, i);
    for (auto& t : Configuration::triggers(cmd)) {
      MidiEvent me = t;
      me.data2 = muted ? 127 : 0;
      midi.sendControl(me);
    }
  }

  firstTime = false;
}

std::ofstream logFile;
void writeLog(const std::string& msg) {
  if (msg.empty()) return;
  logFile << msg;
  if (msg.back() != '\n') logFile << '\n';
}

void setup() {
  if (!Configuration::begin() || Args::configCheckOnly)
    return;

  logFile.open(Args::logFilePath, std::ios_base::app);
  Log::begin(writeLog);

  DisplayThread::start();
  Serial.begin(115200);
  // while (!Serial);

  midi.begin();
  theLoop.begin(playEvent);

  TimingSpec ts = theLoop.getTimingSpec();

  theLoop.enableMidiClock(Args::sendMidiClock);
  if (Args::tempoSet) {
    ts.tempo = Tempo(Args::tempoBPM);
    ts.tempoMode = TempoMode::locked;
  }
  if (Args::tempoRangeSet) {
    ts.lowTempo = Tempo(Args::tempoLowBPM);
    ts.highTempo = Tempo(Args::tempoHighBPM);
  }
  if (Args::receiveMidiClock)
    ts.tempoMode = TempoMode::synced;
  if (Args::meterSet) {
    ts.meter.base = Args::meterBase;
    ts.meter.beats = Args::meterBeats;
    ts.lockedMeter = true;
  }

  theLoop.setTimingSpec(ts);
  if (Args::tempoSet)
    theLoop.setTempo(ts.tempo);
}

void teardown() {
  Loop::allOffNow();
  DisplayThread::end();
  Log::end();
  logFile.close();
}


void loop() {
  static const auto clockStart = std::chrono::steady_clock::now();
  const auto sinceStart = std::chrono::steady_clock::now() - clockStart;

  TimeInterval timeout = timeout = theLoop.advance(
    std::chrono::duration_cast<WallTime>(sinceStart));

  bool received = false;
  MidiEvent ev;
  while (midi.receive(ev)) {
    noteEvent(ev);
    received = true;
  }
  if (received) return; // go 'round the loop again!

  if (DisplayThread::readyForUpdate()) {
    Loop::Status s = theLoop.status();
    DisplayThread::update(s);

    updateMutes(s);
  }

  constexpr TimeInterval minDisplayRefresh = std::chrono::milliseconds(250);
  midi.poll(std::min(timeout, minDisplayRefresh));
}

bool timeToExit = false;

void finish(int sig) {
  timeToExit = 1;
}

int main(int argc, char *argv[]) {
  signal(SIGABRT, finish);
  signal(SIGINT, finish);
  signal(SIGTERM, finish);

  if (!Args::parse(argc, argv))
    return Args::exitCode;

  setup();
  if (!Args::configCheckOnly)
    while (!timeToExit) loop();

  teardown();

  std::cout << "\n\n";
    // since this only exits via a signal, it is nice to print out some
    // newlines so that the user's prompt is at the left margin.

  return 0;
}

