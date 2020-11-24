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
#include "display.h"
#include "looper.h"
#include "message.h"
#include "midi.h"
#include "types.h"


Midi midi;
Loop theLoop;

void playEvent(const MidiEvent& ev) {
  midi.send(ev);
}

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

  displaySetup();
  Serial.begin(115200);
  // while (!Serial);

  midi.begin();
  theLoop.begin(playEvent);
  theLoop.enableMidiClock(Args::sendMidiClock);
}

void teardown() {
  Loop::allOffNow();
  displayClear();
  Log::end();
  logFile.close();
}

void loop() {
  static const auto clockStart = std::chrono::steady_clock::now();
  const auto sinceStart = std::chrono::steady_clock::now() - clockStart;

  TimeInterval dt = theLoop.setTime(std::chrono::duration_cast<WallTime>(sinceStart));
  TimeInterval timeout = timeout = theLoop.advance(dt);

  bool received = false;
  MidiEvent ev;
  while (midi.receive(ev)) {
    noteEvent(ev);
    received = true;
  }
  if (received) return; // go 'round the loop again!

  static auto nextDisplayUpdate =
    std::chrono::steady_clock::duration::zero();

  if (sinceStart >= nextDisplayUpdate) {
    auto millisSinceStart =
      std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart).count();

    Loop::Status s = theLoop.status();
    displayUpdate(millisSinceStart, s);

    const static auto displayRefresh = std::chrono::milliseconds(200);
      // The display taks about 18ms to update. If we update too often, then
      // MIDI responsiveness will suffer. If too slow, the display is choppy.

    nextDisplayUpdate = sinceStart + displayRefresh;
    return; // go 'round the loop again
  }
  else {
    timeout = std::min(timeout,
      std::chrono::duration_cast<TimeInterval>(nextDisplayUpdate - sinceStart));
  }

  // no display, no events... poll for input
  midi.poll(timeout);
}


void finish(int sig) {
  std::cerr << std::endl << std::endl
    << "** Caught signal " << std::dec << sig << ", exiting." << std::endl;
  teardown();
  exit(-1);
}

int main(int argc, char *argv[]) {
  signal(SIGABRT, finish);
  signal(SIGINT, finish);
  signal(SIGTERM, finish);

  if (!Args::parse(argc, argv))
    return Args::exitCode;

  setup();
  if (!Args::configCheckOnly)
    while (true) loop();
  return 0;
}

