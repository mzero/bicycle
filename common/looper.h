#ifndef _INCLUDE_LOOPER_H_
#define _INCLUDE_LOOPER_H_

#include <array>
#include <cstdint>
#include <vector>

#include "cell.h"
#include "types.h"


typedef void (*EventFunc)(const MidiEvent&);
  // TODO: Needs time somehow? delta? absolute?


class Layer {
public:
  Layer();
  ~Layer();

  TimeInterval next() const;
  TimeInterval advance(TimeInterval);

  void addEvent(const MidiEvent&);
  void keep(TimeInterval baseLength = TimeInterval::zero());
  void clear();

  bool muted;
  uint8_t volume;

private:
  Cell* firstCell;
  Cell* recentCell;
  TimeInterval timeSinceRecent;

  TimeInterval length;
  TimeInterval position;

  friend class Loop;
};


class Loop {
public:
  Loop();

  TimeInterval advance(TimeInterval);

  void addEvent(const MidiEvent&);
  void keep();      // arm next layer
  void arm();       // clear whole loop when next event added
  void clear();

  void layerMute(int layer, bool muted);
  void layerVolume(int layer, uint8_t volume);
  void layerArm(int layer);   // start overwriting this layer on next event
  void layerRearm();

  struct LayerStatus {
    TimeInterval length;
    TimeInterval position;
    bool         muted;
  };

  struct Status {
    int   layerCount;
    int   activeLayer;
    bool  armed;
    bool  layerArmed;
    std::array<LayerStatus, 10> layers;
 };

  Status status() const;

  static void begin(EventFunc);
  static TimeInterval setTime(WallTime);

  static void allOffNow();

private:
  bool armed;

  int activeLayer;
  int layerCount;
  bool layerArmed;
  WallTime armedTime;

  std::vector<Layer> layers;
};


#endif // _INCLUDE_LOOPER_H_
