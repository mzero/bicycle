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
  void keep();
  void arm();       // clear when next event added
  void clear();

  static void begin();

private:
  const EventFunc player;

  AbsTime   walltime;

  bool      armed;
  uint8_t   epoch;

  Cell* firstCell;
  Cell* recentCell;
  DeltaTime timeSinceRecent;

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
