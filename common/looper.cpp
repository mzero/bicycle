#include "looper.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

#include "analysis.h"
#include "cell.h"
#include "clock.h"
#include "message.h"
#include "types.h"


#pragma GCC diagnostic error "-Wconversion"
  // this code should be meticulous about conversions

namespace {
  template<typename T>
  inline T clamp(T v, T lo, T hi) {
    return v < lo ? lo : v <= hi ? v : hi;
  }

  const uint8_t unityVolume = 89;

  inline uint8_t scaleVelocity(uint8_t vel, uint8_t vol) {
    return static_cast<uint8_t>(clamp(
      static_cast<uint32_t>(vel) * static_cast<uint32_t>(vol) / unityVolume,
      0u, 127u));
    // why 89? well... it feels about right...
    // and it is a point on the launchpad pro's grid faders
  }


  EventFunc player = nullptr;


  struct AwaitOff {
    Cell* cell;
    EventInterval start;
  };

  std::array<AwaitOff, 128> awaitingOff;

  void finishAwaitingOff(EventInterval now, const MidiEvent& ev) {
    auto& ao = awaitingOff[ev.data1];
    if (ao.cell) {
      auto dNote = now - ao.start;
      if (dNote > maxDuration) dNote = maxDuration;
      if (dNote < minDuration) dNote = minDuration;
      ao.cell->duration = dNote;
      ao.cell = nullptr;
    }
  }

  void startAwaitingOff(EventInterval now, Cell* cell) {
    finishAwaitingOff(now, cell->event);
    auto& ao = awaitingOff[cell->event.data1];
    ao.cell = cell;
    ao.start = now;
  }

  void cancelAwatingOff(const Cell* cell) {
    auto& ao = awaitingOff[cell->event.data1];
    if (ao.cell == cell) {
      ao.cell = nullptr;
    }
  }

  void clearAwatingOff() {
    for (auto& ao : awaitingOff)
      ao = {nullptr, EventInterval::zero()};
  }


  Cell* pendingOff = nullptr;

  // Note: The Pending off logic uses Cell::nextTime for the amount of time left
  // This is because it is higher resolution than duration.

  void playCell(const Cell& cell, bool mute, uint8_t volume) {
    if (mute) return;

    if (cell.event.isNoteOn() && cell.duration >= minDuration) {
      MidiEvent note = cell.event;
      note.data2 = scaleVelocity(note.data2, volume);
      if (note.data2 == 0)
        return;

      Cell* offCell = Cell::alloc();
      if (!offCell)
        return;   // don't play NoteOn if can't allocate NoteOff

      player(note);

      offCell->event = note;
      offCell->event.data2 = 0; // volume 0 makes it a NoteOff
      offCell->nextTime = cell.duration;
      offCell->link(pendingOff);
      pendingOff = offCell;
    } else {
      player(cell.event);
    }
  }

  EventInterval nextPendingOff() {
    EventInterval nextT = forever<EventInterval>;
    for (Cell *p = pendingOff; p; p = p->next())
      nextT = std::min(nextT, p->nextTime);
    return nextT;
  }

  EventInterval playPendingOff(EventInterval dt) {
    EventInterval nextT = forever<EventInterval>;

    for (Cell *p = pendingOff, *q = nullptr; p;) {
      if (dt < p->nextTime) {
        p->nextTime = p->nextTime - dt;
        nextT = std::min(nextT, p->nextTime);
        q = p;
        p = p->next();
      } else {
        player(p->event);

        Cell* n = p->next();
        p->free();

        if (q)  q->link(n);
        else    pendingOff = n;
        p = n;
      }
    }

    return nextT;
  }
}

Layer::Layer()
  : muted(false), volume(unityVolume),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0), length(0), position(0)
  { }

Layer::~Layer()
  { clear(); }

EventInterval Layer::next() const {
  if (!recentCell) return forever<EventInterval>;
  if (recentCell->atEnd()) return forever<EventInterval>;
  return recentCell->nextTime - timeSinceRecent;
}

EventInterval Layer::advance(EventInterval dt) {
  EventInterval nextT = forever<EventInterval>;

  if (recentCell) {
    if (recentCell->atEnd()) {
      if (dt > forever<EventInterval> - timeSinceRecent) {
        clear();
      } else {
        timeSinceRecent += dt;
        length += dt;
        position += dt;
      }
    } else {
      position = (position + dt) % length;

      while (recentCell->nextTime <= timeSinceRecent + dt) {
        // time to move to the next event, and play it

        Cell* nextCell = recentCell->next();

        dt -= recentCell->nextTime - timeSinceRecent;
        timeSinceRecent = EventInterval::zero();
        recentCell = nextCell;
        playCell(*recentCell, muted, volume);
      }

      timeSinceRecent += dt;

      nextT = recentCell->nextTime - timeSinceRecent;
    }
  }
  return nextT;
}

void Layer::addEvent(EventInterval now, const MidiEvent& ev) {
  muted = false;
    // TODO: Should we be doing this? how to communicate back to controller?

  if (ev.isNoteOn()) {
    MidiEvent note = ev;
    note.data2 = scaleVelocity(note.data2, volume);
    player(note);
  } else {
    player(ev);
  }

  Cell* newCell = Cell::alloc();
  if (!newCell) return; // ran out of cells!
  newCell->event = ev;
  newCell->duration = NoteDuration::zero();

  if (ev.isNoteOn())
    startAwaitingOff(now, newCell);

/* FIXME
  if (!recentCell) {
    // first time through, add the "start" note
    Cell* startCell = Cell::alloc();
    if (startCell) {
      startCell->event = { 0x90, 48, 100 };
        // FIXME: should be defined somewhere
      startCell->layer = 0xff; // a magic layer!
      startCell->duration = 3;

      recentCell = startCell;
      firstCell = recentCell;
      Util::playCell(*this, *recentCell);
    }
  }
*/

  if (recentCell) {
    Cell* nextCell = recentCell->next();
    if (nextCell) {
      newCell->link(nextCell);
      newCell->nextTime = recentCell->nextTime - timeSinceRecent;
    }

    recentCell->link(newCell);
    recentCell->nextTime = timeSinceRecent;
  } else {
    firstCell = newCell;
    // FIXME: note "the one" here?
  }

  recentCell = newCell;
  timeSinceRecent = EventInterval::zero();
}

bool Layer::keep() {
  if (!firstCell) return false;
  if (!recentCell) return false;
  if (length == EventInterval::zero()) return false;
    // should never happen... but just in case

  // closing the loop
  recentCell->link(firstCell);
  recentCell->nextTime = timeSinceRecent;
  firstCell = nullptr;

  return true;
}


void Layer::clear() {
  Cell* start = firstCell ? firstCell : recentCell;

  Cell* curr = start;
  while (curr) {
    Cell* doomed = curr;
    curr = doomed->next();
    doomed->free();
    if (curr == start)
      break;
  }

  recentCell = nullptr;
  firstCell = nullptr;

  timeSinceRecent = EventInterval::zero();
  length = EventInterval::zero();
  position = EventInterval::zero();

  muted = false;
  volume = 100;
    // TODO: How to communicate back to controller?
}

bool Layer::looping() const {
  return firstCell == nullptr;
}

bool Layer::empty() const {
  return recentCell == nullptr;
}

void Layer::retime(const Tempo& from, const Tempo& to) {
  double r = Tempo::retimeRate(from, to);
  fprintf(stderr, "retime ratio is %8.4f\n", r);

  Cell* start = firstCell ? firstCell : recentCell;

  auto p = start;
  EventInterval oldEventStart(0);
  EventInterval newEventStart(0);
  while (p) {
    EventInterval oldEventEnd = oldEventStart + p->duration;
    EventInterval newEventEnd = oldEventEnd.retime(r);

    EventInterval oldNextEvent = oldEventStart + p->nextTime;
    EventInterval newNextEvent = oldNextEvent.retime(r);

    Cell oldCell = *p;

    p->duration = NoteDuration(newEventEnd - newEventStart);
      // FIXME: limit to legal range of durations
    p->nextTime = newNextEvent - newEventStart;

    fprintf(stderr, "  @ %6d/%6d: dur = %6d/%6d (Δ%6d/%6d) next = %6d/%6d (Δ%6d/%6d)\n",
      oldEventStart.count(), newEventStart.count(),
      oldEventEnd.count(), newEventEnd.count(),
      oldCell.duration.count(), p->duration.count(),
      oldNextEvent.count(), newNextEvent.count(),
      oldCell.nextTime.count(), p->nextTime.count());

    oldEventStart = oldNextEvent;
    newEventStart = newNextEvent;

    p = p->next();
    if (p == start) break;
  }

  fprintf(stderr, "  old: len %6d, pos %6d, tsr %6d\n",
    length.count(), position.count(), timeSinceRecent.count());

  length = newEventStart;
  position = position.retime(r);
  timeSinceRecent = timeSinceRecent.retime(r);

  fprintf(stderr, "  new: len %6d, pos %6d, tsr %6d\n",
    length.count(), position.count(), timeSinceRecent.count());

}

// FIXME: There is a relationship between keep() and resize() - they should
// be called in order to "finalize" a layer.  This should be better refactored
// so that Layer can't end up in a bad state.

void Layer::resize(EventInterval baseLength) {
  EventInterval adj = syncLength(baseLength, length, timeSinceRecent);

  fprintf(stderr, "Layer::resize: resize(%d) adjusting by %d\n",
    baseLength.count(), adj.count());

  recentCell->nextTime += adj;
  length += adj;
  position = position % length;

  // advance into the start of the loop
  advance(adj < EventInterval::zero() ? -adj : EventInterval::zero());
}

Loop::Loop()
  : armed(true), layerCount(0), activeLayer(0), layerArmed(false),
    layers(10),
    epochTempo(120)
  {
    clearAwatingOff();
  }


TimeInterval Loop::advance(WallTime w) {
  if (w < nowWall) {
    // Rollover of walltime?!
    epochWall = w;
    epochTime = nowTime;
    nowWall = w;
    return TimeInterval::zero();
  }

  auto elapsedWall = std::chrono::duration_cast<TimeInterval>(w - epochWall);
  auto newTime = epochTime + epochTempo.toEventInterval(elapsedWall);
  auto dT = newTime - nowTime;

  // These updated just once at the start, not as the playing progresses
  // in the code below.
  nowTime = newTime;
  nowWall = w;
    // N.B.: This isn't _exactly_ where nowTime is.  That would be:
    // epochWall + epochTempo.toTimeInterval(nowTime - epochTime)


  EventInterval nextT = nextPendingOff();
  nextT = std::min(nextT, clockLayer.next());
  for (auto& l : layers)
    nextT = std::min(nextT, l.next());

  while (dT > EventInterval::zero()) {
    EventInterval stepT = std::min(dT, nextT);

    nextT = playPendingOff(stepT);
    nextT = std::min(nextT, clockLayer.advance(stepT));
    for (auto& l : layers)
      nextT = std::min(nextT, l.advance(stepT));

    dT -= stepT;
  }

  return epochTempo.toTimeInterval(nextT);
}


void Loop::addEvent(const MidiEvent& ev) {
  if (ev.isNoteOff()) {
    // note off processing
    player(ev);
    finishAwaitingOff(nowTime, ev);
    return;
  }

  if (armed) {
    resetLayers();

    epochWall = nowWall;
    epochTime = EventInterval::zero();
    nowTime = epochTime;

    armed = false;

    kickClock();
  }

  Layer& l = layers[activeLayer];   // TODO: bounds check

  if (layerArmed) {
    l.clear();
    layerArmed = false;
  }

  l.addEvent(nowTime, ev);
}


void Loop::keep() {
  Layer& l = layers[activeLayer];   // TODO: bounds check

  if (!l.keep())
    return;

  if (layerCount == 1) {
    if (timingSpec.tempoMode == TempoMode::inferred) {
      TimeSignature ts = estimateTimeSignature(
          timingSpec, l.length, l.recentCell);

      l.retime(epochTempo, ts.tempo);
      setTempo(ts.tempo);

      if (!timingSpec.lockedMeter)
        timingSpec.meter = ts.meter;

      l.resize(EventInterval::zero());

      clockLayer.syncStart(l.length);

    } else {
      if (!timingSpec.lockedMeter) {
        timingSpec.meter.beats = 1;
        l.resize(timingSpec.baseLength());
        timingSpec.meter.beats =
          l.length.count() / timingSpec.baseLength().count();
      }
      else {
        l.resize(timingSpec.baseLength());
      }

      clockLayer.resize(timingSpec.baseLength());
    }


    fprintf(stderr, "first layer: %6.2fbpm, %d/%d\n",
      timingSpec.tempo.inBPM(), timingSpec.meter.beats, timingSpec.meter.base);
  }
  else {
    l.resize(timingSpec.baseLength());
  }

  activeLayer += activeLayer < (layers.size() - 1) ? 1 : 0;
  layerArmed = true;
  layerCount = std::max(layerCount, activeLayer + 1);
}

void Loop::arm() {
  armed = true;
  kickClock();
}

void Loop::kickClock() {
  if (timingSpec.tempoMode != TempoMode::inferred  &&  !clockLayer.isRunning())
    clockLayer.syncStart(forever<EventInterval>);
}

void Loop::clear() {
  clockLayer.stop();
  clearAwatingOff();

  resetLayers();

  Message::clear();
}

void Loop::resetLayers() {
  for (auto& l : layers)
    l.clear();

  armed = true;
  layerCount = 1;
  activeLayer = 0;
  layerArmed = true;
}


void Loop::layerMute(int layer, bool muted) {
  if (layer < layers.size()) layers[layer].muted = muted;
}

void Loop::layerVolume(int layer, uint8_t volume) {
  if (layer < layers.size()) layers[layer].volume = volume;
}

void Loop::layerArm(int layer) {
  if (!(0 <= layer && layer < layers.size())) return;

  if (layerArmed && activeLayer == layer
      && nowWall < (armedTime + std::chrono::seconds(1))) {
    // double press of the layer arm control
    layers[activeLayer].clear();
    return;
  }

  if (!layers[layer].looping()) {
    // if use rearms before keep, just trash what they recorded
    layers[layer].clear();
  }

  // FIXME: what to do if still recording initial layer?
  activeLayer = layer;
  layerArmed = true;
  armedTime = nowWall;

  layerCount = std::max(layerCount, activeLayer + 1);
}

void Loop::layerRearm() {
  layerArm(activeLayer);
}


Tempo Loop::getTempo() const {
  return epochTempo;
}

void Loop::setTempo(Tempo newTempo) {
  // Carefully reset the epoch, using the current tempo, to where
  // the metric time is now.  N.B.: Don't use nowWall, as that is not
  // kept in perfect sync with nowTime. (See advance() for details.)
  epochWall += epochTempo.toTimeInterval(nowTime - epochTime);
  epochTime = nowTime;
  epochTempo = newTempo;

  timingSpec.tempo = newTempo;
}

TimingSpec Loop::getTimingSpec() const {
  return timingSpec;
}

void Loop::setTimingSpec(const TimingSpec& ts) {
  timingSpec = ts;
  timingSpec.tempo = epochTempo;

  {
    Message msg;
    switch (ts.tempoMode) {
      case TempoMode::inferred:
        msg << int(ts.lowTempo.inBPM()) << '~' << int(ts.highTempo.inBPM());
        break;
      case TempoMode::locked:
        msg << int(ts.tempo.inBPM()) << '!';
        break;
      case TempoMode::synced:
        msg << "sync";
        break;
      default:
        msg << int(ts.tempo.inBPM()) << '!';
    }

    msg << ' ' << ts.meter.beats << '/' << ts.meter.base;
    if (ts.lockedMeter)
      msg << '!';
  }
}

void Loop::enableMidiClock(bool enable) {
  ClockLayer::setPlayer(enable ? player : nullptr);
}

Loop::Status Loop::status() const {
  Status s;
  s.layerCount = layerCount;
  s.activeLayer = activeLayer;
  s.armed = armed;
  s.layerArmed = layerArmed;

  for (int i = 0; i < s.layers.size(); ++i) {
    auto& sl = s.layers[i];
    if (i < layers.size()) {
      auto& l = layers[i];
      sl.length = l.length;
      sl.position = l.position;
      sl.muted = l.muted;
    } else {
      sl.length = EventInterval::zero();
      sl.position = EventInterval::zero();
      sl.muted = false;
    }
  }

  return s;
}

void Loop::begin(EventFunc p) {
  player = p;
  Cell::begin();
  ClockLayer::setPlayer(p);
}

void Loop::allOffNow() {
  playPendingOff(forever<EventInterval>);
}
