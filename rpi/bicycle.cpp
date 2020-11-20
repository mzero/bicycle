#include <Arduino.h>

#include <fstream>

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

const char* logFilePath = "_bicycle_log";
std::ofstream logFile;
void writeLog(const std::string& msg) {
  if (msg.empty()) return;
  logFile << msg;
  if (msg.back() != '\n') logFile << '\n';
}

void setup() {
  logFile.open(logFilePath, std::ios_base::app);
  Log::begin(writeLog);

  Configuration::begin();

  displaySetup();
  Serial.begin(115200);
  // while (!Serial);

  midi.begin();
  theLoop.begin(playEvent);
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
  const auto sinceStartMillis = std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart);
  const auto millis = sinceStartMillis.count();

  TimeInterval timeout = forever;
  {
    TimeInterval dt = theLoop.setTime(std::chrono::duration_cast<WallTime>(sinceStart));
    timeout = theLoop.advance(dt);
  }

  MidiEvent ev;
  while (midi.receive(ev, timeout)) {
    noteEvent(ev);
  }

  // analogUpdate(millis);

  Loop::Status s = theLoop.status();
  displayUpdate(millis, s);
}
