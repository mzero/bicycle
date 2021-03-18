#ifndef _INCLUDE_LOOPER_H_
#define _INCLUDE_LOOPER_H_

#include <array>
#include <cstdint>
#include <vector>

#include "cell.h"
#include "metrictime.h"
#include "types.h"


typedef void (*EventFunc)(const MidiEvent&);
  // TODO: Needs time somehow? delta? absolute?


class Layer {
public:
  Layer();
  ~Layer();

  EventInterval next() const;
  EventInterval advance(EventInterval);

  void addEvent(EventInterval, const MidiEvent&);
  bool keep(EventInterval baseLength = EventInterval::zero());
  void clear();
  bool looping() const;

  void retime(const Tempo& from, const Tempo& to);

  bool muted;
  uint8_t volume;

private:
  Cell* firstCell;
  Cell* recentCell;
  EventInterval timeSinceRecent;

  EventInterval length;
  EventInterval position;

  friend class Loop;
};


class Loop {
public:
  Loop();

  TimeInterval advance(WallTime now);

  void addEvent(const MidiEvent&);
  void keep();      // arm next layer
  void arm();       // clear whole loop when next event added
  void clear();

  void layerMute(int layer, bool muted);
  void layerVolume(int layer, uint8_t volume);
  void layerArm(int layer);   // start overwriting this layer on next event
  void layerRearm();

  void enableMidiClock(bool);
  void setTempo(const Tempo&);
  void setMeter(const Meter&);

  struct LayerStatus {
    EventInterval length;
    EventInterval position;
    bool          muted;
  };

  struct Status {
    int   layerCount;
    int   activeLayer;
    bool  armed;
    bool  layerArmed;

    static constexpr int numLayers = 10;
    std::array<LayerStatus, numLayers> layers;
 };

  Status status() const;

  static void begin(EventFunc);
  static void allOffNow();

private:
  WallTime      epochWall;
  EventInterval epochTime;
  Tempo         epochTempo;

  WallTime      nowWall;
  EventInterval nowTime;

  Meter meter;

  bool midiClock;
  bool armed;

  int activeLayer;
  int layerCount;
  bool layerArmed;
  WallTime armedTime;

  std::vector<Layer> layers;
};


#endif // _INCLUDE_LOOPER_H_
