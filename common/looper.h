#ifndef _INCLUDE_LOOPER_H_
#define _INCLUDE_LOOPER_H_

#include <array>
#include <cstdint>
#include <vector>

#include "cell.h"
#include "types.h"


const DeltaTime maxEventInterval = 20000;
  // maximum amount of time spent waiting for a new event


typedef void (*EventFunc)(const MidiEvent&);
  // TODO: Needs time somehow? delta? absolute?


class Layer {
public:
  Layer();
  ~Layer();

  AbsTime next() const;
  AbsTime advance(AbsTime);

  void addEvent(const MidiEvent&);
  void keep(AbsTime baseLength = 0);
  void clear();

  bool muted;
  uint8_t volume;

private:
  Cell* firstCell;
  Cell* recentCell;
  DeltaTime timeSinceRecent;

  AbsTime length;
  AbsTime position;

  friend class Loop;
};


class Loop {
public:
  Loop();

  AbsTime advance(AbsTime);

  void addEvent(const MidiEvent&);
  void keep();      // arm next layer
  void arm();       // clear whole loop when next event added
  void clear();

  void layerMute(uint8_t layer, bool muted);
  void layerVolume(uint8_t layer, uint8_t volume);
  void layerArm(uint8_t layer);   // start overwriting this layer on next event
  void layerRearm();

  struct LayerStatus {
    AbsTime     length;
    AbsTime     position;
    bool        muted;
  };

  struct Status {
    uint8_t     layerCount;
    uint8_t     activeLayer;
    bool        armed;
    bool        layerArmed;
    std::array<LayerStatus, 10> layers;
 };

  Status status() const;

  static void begin(EventFunc);
  static AbsTime setTime(AbsTime);

private:
  bool armed;

  uint8_t activeLayer;
  uint8_t layerCount;
  bool layerArmed;
  AbsTime armedTime;

  std::vector<Layer> layers;
};


#endif // _INCLUDE_LOOPER_H_
