#ifndef _INCLUDE_LOOPER_H_
#define _INCLUDE_LOOPER_H_

#include <array>
#include <cstdint>
#include <vector>

#include "cell.h"
#include "metrictime.h"
#include "types.h"


class Layer {
public:
  Layer();
  ~Layer();

  EventInterval next() const;
  EventInterval advance(EventInterval);

  void addEvent(EventInterval, const MidiEvent&);
  bool keep();
  void clear();

  bool looping() const;
  bool empty() const;

  void retime(const Tempo& from, const Tempo& to);
  void resize(EventInterval baseLength);

  bool muted;
  uint8_t volume;

private:
  Cell* firstCell;
  Cell* recentCell;
  EventInterval timeSinceRecent;

  EventInterval length;
  EventInterval position;

  int channel;

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

  Tempo getTempo() const;
  void setTempo(Tempo);

  TimingSpec getTimingSpec() const;
  void setTimingSpec(const TimingSpec&);
    // note: does not change tempo, use setTempo()

  void enableMidiClock(bool);

  struct LayerStatus {
    EventInterval length;
    EventInterval position;
    int           channel;
    bool          muted;
  };

  struct Status {
    Tempo tempo;
    Meter meter;

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

  TimingSpec    timingSpec;

  bool armed;

  int activeLayer;
  int layerCount;
  bool layerArmed;
  WallTime armedTime;

  std::vector<Layer> layers;

  void resetLayers();
  void kickClock();
};


#endif // _INCLUDE_LOOPER_H_
