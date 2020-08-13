#ifndef _INCLUDE_LOOPER_H_
#define _INCLUDE_LOOPER_H_

#include <cstdint>


typedef uint16_t DeltaTime;
typedef uint8_t MidiEvent[3];

typedef void (*EventFunc)(const MidiEvent&);
  // TODO: Needs time somehow? delta? absolute?

class Loop {
public:
  Loop();

  void advance(DeltaTime, EventFunc);
  void addEvent(const MidiEvent&);
  void keep();
  void arm();
  void clear();

  static void begin();

private:
  bool      armed;   // clear when next event added
  uint8_t   epoch;

  class Cell;
  Cell* firstCell;
  Cell* recentCell;
  DeltaTime timeSinceRecent;
};


#endif // _INCLUDE_LOOPER_H_
