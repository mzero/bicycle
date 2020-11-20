#include "configuration.h"

#include "eventmap.h"

namespace {
    EventMap<Command> noteMap(Command(Action::none));
    EventMap<Command> ccMap(Command(Action::none));
}

void Configuration::begin() {

    // Ignore everything on the "control" channel unless otherwise set

    for (int i = 0; i < 128; ++i) {
      ccMap.set(15, i, Command(Action::ignore));
      noteMap.set(15, i, Command(Action::ignore));
    }

    // Mapping for nanoKONTROL & Launchpad Pro MK3 w/custom layout

    ccMap.set(15,  2, Command(Action::layerVolume, 0));
    ccMap.set(15,  3, Command(Action::layerVolume, 1));
    ccMap.set(15,  4, Command(Action::layerVolume, 2));
    ccMap.set(15,  5, Command(Action::layerVolume, 3));
    ccMap.set(15,  6, Command(Action::layerVolume, 4));
    ccMap.set(15,  8, Command(Action::layerVolume, 5));
    ccMap.set(15,  9, Command(Action::layerVolume, 6));
    ccMap.set(15, 11, Command(Action::layerVolume, 7));
    ccMap.set(15, 12, Command(Action::layerVolume, 8));
          // yes, CCs 7, 10, & 11 are skipped

    ccMap.set(15, 23, Command(Action::layerMute, 0));
    ccMap.set(15, 24, Command(Action::layerMute, 1));
    ccMap.set(15, 25, Command(Action::layerMute, 2));
    ccMap.set(15, 26, Command(Action::layerMute, 3));
    ccMap.set(15, 27, Command(Action::layerMute, 4));
    ccMap.set(15, 28, Command(Action::layerMute, 5));
    ccMap.set(15, 29, Command(Action::layerMute, 6));
    ccMap.set(15, 30, Command(Action::layerMute, 7));
    ccMap.set(15, 31, Command(Action::layerMute, 8));

    ccMap.set(15, 33, Command(Action::layerArm, 0));
    ccMap.set(15, 34, Command(Action::layerArm, 1));
    ccMap.set(15, 35, Command(Action::layerArm, 2));
    ccMap.set(15, 36, Command(Action::layerArm, 3));
    ccMap.set(15, 37, Command(Action::layerArm, 4));
    ccMap.set(15, 38, Command(Action::layerArm, 5));
    ccMap.set(15, 39, Command(Action::layerArm, 6));
    ccMap.set(15, 40, Command(Action::layerArm, 7));
    ccMap.set(15, 41, Command(Action::layerArm, 8));

    ccMap.set(15, 44, Command(Action::arm));    // REC
    ccMap.set(15, 45, Command(Action::keep));   // PLAY
    ccMap.set(15, 46, Command(Action::clear));  // STOP
    ccMap.set(15, 47, Command(Action::bad));    // REW
    ccMap.set(15, 48, Command(Action::good));   // FF
    ccMap.set(15, 49, Command(Action::rearm));  // LOOP

    // Mapping for Boppad

    noteMap.set(9, 48, Command(Action::keep));  // upper left

    // Mapping for LaunchPad Pro MK3 in Note mode
    //  -- these are the side buttons, which come on the :2 port

    // left side, bottom upward
    ccMap.set(0, 10, Command(Action::keep));
    ccMap.set(0, 20, Command(Action::rearm));
    ccMap.set(0, 30, Command(Action::bad));
    ccMap.set(0, 40, Command(Action::good));
    ccMap.set(0, 50, Command(Action::arm));
    ccMap.set(0, 60, Command(Action::clear));

    // General input controllers

    ccMap.setAllChannels(64, Command(Action::keep));
        // treat sustain pedal as the keep function
}

Command Configuration::command(const MidiEvent& ev) {
  switch (ev.status & 0xf0) {
    case 0x90:    return noteMap.get(ev);
    case 0xb0:    return ccMap.get(ev);
    default:      return Command(Action::none);
  }
}

