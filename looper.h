#ifndef _INCLUDE_LOOPER_H_
#define _INCLUDE_LOOPER_H_

#include <array>
#include <cstdint>

#include "cell.h"
#include "types.h"


const DeltaTime maxEventInterval = 20000;
  // maximum amount of time spent waiting for a new event


typedef void (*EventFunc)(const MidiEvent&);
  // TODO: Needs time somehow? delta? absolute?




class Loop {
public:
  Loop(EventFunc);

  void advance(AbsTime);
  void addEvent(const MidiEvent&);
  void keep();      // arm next layer
  void arm();       // clear whole loop when next event added
  void clear();

  void layerMute(uint8_t layer, bool muted);
  void layerVolume(uint8_t layer, uint8_t volume);
  void layerArm(uint8_t layer);   // start overwriting this layer on next event


  struct Status {
    AbsTime     length;
    AbsTime     position;
    uint8_t     activeLayer;
    bool        looping;
    bool        armed;
    bool        layerArmed;
  };

  Status status() const;

  static void begin();

private:
  const EventFunc player;

  AbsTime   walltime;

  bool      armed;

  uint8_t activeLayer;
  bool layerArmed;
  std::array<bool, 9> layerMutes;
  std::array<uint8_t, 9> layerVolumes;

  Cell* firstCell;
  Cell* recentCell;
  DeltaTime timeSinceRecent;

  AbsTime length;
  AbsTime position;

  struct AwaitOff {
    Cell* cell;
    AbsTime start;
  };

  std::array<AwaitOff, 128> awaitingOff;

  Cell* pendingOff;

  class Util;
  friend class Util;
};


#endif // _INCLUDE_LOOPER_H_
