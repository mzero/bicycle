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

  DisplayThread::start();
  Serial.begin(115200);
  // while (!Serial);

  midi.begin();
  theLoop.begin(playEvent);
  theLoop.enableMidiClock(Args::sendMidiClock);
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

  TimeInterval dt = theLoop.setTime(std::chrono::duration_cast<WallTime>(sinceStart));
  TimeInterval timeout = timeout = theLoop.advance(dt);

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

