#include "looper.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

#include "cell.h"
#include "message.h"
#include "types.h"


#pragma GCC diagnostic error "-Wconversion"
  // this code should be meticulous about conversions

namespace {
  template<typename T>
  inline T clamp(T v, T lo, T hi) {
    return v < lo ? lo : v <= hi ? v : hi;
  }

  inline uint8_t scaleVelocity(uint8_t vel, uint8_t vol) {
    return static_cast<uint8_t>(clamp(
      static_cast<uint32_t>(vel) * static_cast<uint32_t>(vol) / 100,
      0u, 127u));
  }

  TimeInterval syncLength(
      TimeInterval base, TimeInterval len, TimeInterval maxShorten) {
    const int limit = 7;

    if (base == TimeInterval::zero())
      return TimeInterval::zero();

    Log log;
    log << "layer sync: "
      << base.count() << " :: " << len.count()
      << " (" << maxShorten.count() << ") = ";

    double b = base.count();
    double l = len.count();
    double x = 0.0 - maxShorten.count();

    int nBest = 0;
    int mBest = 0;
    double errBest = INFINITY;

    for (int i = 1; i < limit; ++i) {
      int n;
      int m;

      if (b < l) {
        n = std::lround(i * l / b);
        m = i;
      } else {
        n = i;
        m = std::lround(i * b / l);
      }
      if (i > 1 && (n >= limit || m >= limit)) continue;

      double err = (n * b - m * l) / m;
      if (err > x && std::abs(err) < std::abs(errBest)) {
        nBest = n;
        mBest = m;
        errBest = err;
      }
    }
    Message msg;
    if (nBest == 0) {
      log << "failed to find relationship";
      msg << "?:?";
      return TimeInterval::zero();
    }

    int adj = lround(errBest);
    log << nBest << ":" << mBest << ", adjusting by " << adj;
    msg << nBest << ":" << mBest;
    return TimeInterval(adj);
  }

  float beatError(float x) {
    float e = 0.0f;
    for (int i = 0; i < 8; ++i) {
      x = x - std::trunc(x);
      e += x >= 0.5f ? (1.0f - x) : x;
      x = x + x;
    }
    return e;
  }

  int estimateMeter(TimeInterval base, const Cell* firstCell) {
    constexpr float minBPM = 75; // 85;
    constexpr float maxBPM = 140; // 2 * minBPM;

    using Seconds = std::chrono::duration<float>;
    constexpr Seconds maxQnote = std::chrono::minutes(1) / minBPM;
    constexpr Seconds minQnote = std::chrono::minutes(1) / maxBPM;

    Seconds basef(base);

    int minMN = int(ceilf(basef / maxQnote));
    int maxMN = int(floorf(basef / minQnote));

    int bestMN = 0;
    float bestErr = INFINITY;

    for (int mn = minMN; mn <= maxMN; ++mn) {
      const Seconds qn = basef / mn;
      const float qnf = qn.count();

      float err = 0.0f;
      auto p = firstCell;
      TimeInterval t(0);
      while (p) {
        float tf = Seconds(t).count();
        err += beatError(tf / qnf);
        if (err > bestErr) break;

        t += p->nextTime;
        p = p->next();
        if (p == firstCell) break;
      }

      if (err < bestErr) {
        bestMN = mn;
        bestErr = err;
      }
    }

    {
      Log log;
      log << "layer deltas: ";
      auto p = firstCell;
      while (p) {
        log << p->nextTime.count() << ",";
        p = p->next();
        if (p == firstCell) break;
      }
    }

    if (bestMN == 0)
      bestMN = 1; // should never happen, but just to be safe

    {
      Log log;
      float bestBPM = std::chrono::minutes(1) / (basef / bestMN);
      log << "meter: " << bestMN << "beats ("
        << minMN << "," << maxMN << ") @ " << bestBPM << " bpm";

      Message msg;
      msg << bestMN << " @ " << static_cast<int>(bestBPM) << " bpm";
    }

    return bestMN;
  }

  WallTime walltime(0);

  EventFunc player;


  struct AwaitOff {
    Cell* cell;
    WallTime start;
  };

  std::array<AwaitOff, 128> awaitingOff;


  void finishAwaitingOff(const MidiEvent& ev) {
    auto& ao = awaitingOff[ev.data1];
    if (ao.cell) {
      auto dWall = walltime - ao.start;
      if (dWall > maxDuration) dWall = maxDuration;
      auto dNote = std::chrono::duration_cast<NoteDuration>(dWall);
      if (dNote < minDuration) dNote = minDuration;
      ao.cell->duration = dNote;
      ao.cell = nullptr;
    }
  }

  void startAwaitingOff(Cell* cell) {
    finishAwaitingOff(cell->event);
    auto& ao = awaitingOff[cell->event.data1];
    ao.cell = cell;
    ao.start = walltime;
  }

  void cancelAwatingOff(const Cell* cell) {
    auto& ao = awaitingOff[cell->event.data1];
    if (ao.cell == cell) {
      ao.cell = nullptr;
    }
  }

  void clearAwatingOff() {
    for (auto& ao : awaitingOff)
      ao = {nullptr, WallTime::zero()};
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

  TimeInterval playPendingOff(TimeInterval dt) {
    TimeInterval nextT = forever;

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

  const MidiEvent clockEvent(0xf8);
  const MidiEvent startEvent(0xfa);
  const MidiEvent continueEvent(0xfb);
  const MidiEvent stopEvent(0xfc);

  class ClockLayer {
  public:
    ClockLayer() : running(false) { }

    void set(TimeInterval length_, int beats) {
      length = length_;
      clockCount = beats * 24;
      clock = length / clockCount;

      position = TimeInterval::zero();
      nextClock = TimeInterval::zero();
      clockNumber = 0;

      running = true;
    }

    void clear() {
      running = false;
      player(stopEvent);
    }

    TimeInterval next() const {
      return running ? nextClock : forever;
    }

    TimeInterval advance(TimeInterval dt) {
      if (!running) return forever;

      while (nextClock <= dt) {
        dt -= nextClock;
        position = (position + nextClock) % length;

        if (clockNumber == 0) {
          player(stopEvent);
          player(startEvent);
        }
        player(clockEvent);

        clockNumber += 1;
        if (clockNumber == clockCount) {
          clockNumber = 0;
          nextClock = length - position;
        } else {
          nextClock = clock;
        }

      }
      nextClock -= dt;
      position += dt;  // no need to % length, can't roll here!
      return nextClock;
    }

  private:
    bool running;

    TimeInterval length;
    int clockCount;
    TimeInterval clock;

    TimeInterval position;
    TimeInterval nextClock;
    int clockNumber;
  };

  ClockLayer clockLayer;
}

Layer::Layer()
  : muted(false), volume(100),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0), length(0), position(0)
  { }

Layer::~Layer()
  { clear(); }

TimeInterval Layer::next() const {
  if (!recentCell) return forever;
  if (recentCell->atEnd()) return forever;
  return recentCell->nextTime - timeSinceRecent;
}

TimeInterval Layer::advance(TimeInterval dt) {
  TimeInterval nextT = forever;

  if (recentCell) {
    if (recentCell->atEnd()) {
      if (dt > TimeInterval::max() - timeSinceRecent) {
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
        timeSinceRecent = TimeInterval::zero();
        recentCell = nextCell;
        playCell(*recentCell, muted, volume);
      }

      timeSinceRecent += dt;

      nextT = recentCell->nextTime - timeSinceRecent;
    }
  }
  return nextT;
}

void Layer::addEvent(const MidiEvent& ev) {
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
    startAwaitingOff(newCell);

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
  timeSinceRecent = TimeInterval::zero();
}

void Layer::keep(TimeInterval baseLength) {
  if (!firstCell) return;

  TimeInterval adj = syncLength(baseLength, length, timeSinceRecent);

  // closing the loop
  recentCell->link(firstCell);
  recentCell->nextTime = timeSinceRecent + adj;
  firstCell = nullptr;

  length += adj;
  position = position % length;

  // advance into the start of the loop
  advance(adj < TimeInterval::zero() ? -adj : TimeInterval::zero());
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

  timeSinceRecent = TimeInterval::zero();
  length = TimeInterval::zero();
  position = TimeInterval::zero();

  muted = false;
  volume = 100;
    // TODO: How to communicate back to controller?
}

Loop::Loop()
  : armed(true), layerCount(0), activeLayer(0), layerArmed(false),
    layers(10)
  {
    clearAwatingOff();
  }


TimeInterval Loop::advance(TimeInterval dt) {
  // In theory the offs should be interleaved as we go through the next
  // set of cells to play. BUT, since dt has already elapsed, it is roughly
  // okay to just spit out the NoteOff events first. And anyway, dt is rarely
  // more than 1ms.

  TimeInterval nextT = playPendingOff(dt);
  nextT = std::min(nextT, clockLayer.next());
  for (auto& l : layers)
    nextT = std::min(nextT, l.next());

  while (dt > TimeInterval::zero()) {
    TimeInterval et = std::min(dt, nextT);

    nextT = clockLayer.advance(et);
    for (auto& l : layers)
      nextT = std::min(nextT, l.advance(et));

    dt -= et;
  }

  return nextT;
}


void Loop::addEvent(const MidiEvent& ev) {
  if (ev.isNoteOff()) {
    // note off processing
    player(ev);
    finishAwaitingOff(ev);
    return;
  }

  if (armed) {
    clear();
    armed = false;
  }

  Layer& l = layers[activeLayer];   // TODO: bounds check

  if (layerArmed) {
    l.clear();
    layerArmed = false;
  }

  l.addEvent(ev);
}


void Loop::keep() {
  Layer& l = layers[activeLayer];   // TODO: bounds check

  l.keep(layers[activeLayer ? 0 : 1].length);
  if (layerCount == 1) {
    auto beats = estimateMeter(l.length, l.recentCell);
    clockLayer.set(l.length, beats);
  }
  activeLayer += activeLayer < (layers.size() - 1) ? 1 : 0;
  layerArmed = true;
  layerCount = std::max(layerCount, activeLayer + 1);
}

void Loop::arm() {
  armed = true;
}

void Loop::clear() {
  clockLayer.clear();
  clearAwatingOff();

  for (auto& l : layers)
    l.clear();

  armed = true;
  layerCount = 1;
  activeLayer = 0;
  layerArmed = true;

  Message::clear();
}


void Loop::layerMute(int layer, bool muted) {
  if (layer < layers.size()) layers[layer].muted = muted;
}

void Loop::layerVolume(int layer, uint8_t volume) {
  if (layer < layers.size()) layers[layer].volume = volume;
}

void Loop::layerArm(int layer) {
  if (layerArmed && activeLayer == layer
      && walltime < (armedTime + std::chrono::seconds(1))) {
    // double press of the layer arm control
    layers[activeLayer].clear();
    return;
  }

  // FIXME: what to do if still recording initial layer?
  activeLayer = layer;
  layerArmed = true;
  armedTime = walltime;

  layerCount = std::max(layerCount, activeLayer + 1);
}

void Loop::layerRearm() {
  layerArm(activeLayer);
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
      sl.length = TimeInterval::zero();
      sl.position = TimeInterval::zero();
      sl.muted = false;
    }
  }

  return s;
}

void Loop::begin(EventFunc p) {
  player = p;
  Cell::begin();
}

TimeInterval Loop::setTime(WallTime now) {
  static WallTime pending(0);

  if (now < walltime) return TimeInterval::zero();
    // FIXME: Handle rollover of walltime?

  pending += now - walltime;
  walltime = now;

  auto dt = std::chrono::duration_cast<TimeInterval>(pending);
  pending -= dt;
  return dt;
}

void Loop::allOffNow() {
  playPendingOff(forever);
}
