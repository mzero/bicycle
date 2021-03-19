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

enum class TempoMode {
  begin = 0,
  inferred = begin,
  locked,
  synced,
  end
};

struct TimingSpec {
  Tempo         tempo;        // tempo currently playing
  Tempo         lowTempo;     // low range of tempo estimates
  Tempo         highTempo;    // high range of tempo estimates

  TempoMode     tempoMode;

  Meter         meter;        // meter currently playing
  bool          lockedMeter;  // layer timing will be locked to given meter

  TimingSpec()
    : tempo(120.0), lowTempo(75.0), highTempo(140.0),
      tempoMode(TempoMode::inferred),
      meter{4, 4},
      lockedMeter(false)
      { }

  EventInterval baseLength() const {
    using Factor = std::ratio_divide<WholeNotes, EventInterval::Units>;
    return EventInterval(meter.beats * Factor::num / meter.base / Factor::den);
  }
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

  TimingSpec    timingSpec;

  bool midiClock;
  bool armed;

  int activeLayer;
  int layerCount;
  bool layerArmed;
  WallTime armedTime;

  std::vector<Layer> layers;
};


#endif // _INCLUDE_LOOPER_H_
