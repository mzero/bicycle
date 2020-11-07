#include "looper.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

#include "cell.h"


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

  int syncLength(AbsTime base, AbsTime len, AbsTime maxShorten) {
    const int limit = 7;

    std::cout << "sync: " << base << " :: " << len << " (" << maxShorten << ")\n";

    double b = base;
    double l = len;
    double x = 0.0 - maxShorten;

    int nBest = 0;
    int mBest = 0;
    double errBest = INFINITY;

    for (int i = 1; i < limit; ++i) {
      int n;
      int m;

      if (b < l) {
        n = std::round(i * l / b);
        m = i;
      } else {
        n = i;
        m = std::round(i * b / l);
      }
      if (i > 1 && (n >= limit || m >= limit)) continue;

      double err = (n * b - m * l) / m;
      std::cout << "    trying " << n << ":" << m << " gives err " << err << '\n';
      if (err > x && std::abs(err) < std::abs(errBest)) {
        nBest = n;
        mBest = m;
        errBest = err;
      }
    }

    if (nBest == 0) {
      std::cout << "    failed to find relationship\n";
      return 0;
    }

    int adj = round(errBest);
    std::cout << "    " << nBest << ":" << mBest << ", adjusting by " << adj << "\n";
    return adj;
  }

  AbsTime walltime = 0;

  EventFunc player;


  struct AwaitOff {
    Cell* cell;
    AbsTime start;
  };

  std::array<AwaitOff, 128> awaitingOff;

  Cell* pendingOff = nullptr;


  void finishAwaitingOff(const MidiEvent& ev) {
    auto& ao = awaitingOff[ev.data1];
    if (ao.cell) {
      ao.cell->duration = walltime - ao.start;
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
      ao = {nullptr, 0};
  }


  void playCell(const Cell& cell, bool mute, uint8_t volume) {
    if (mute) return;

    if (cell.event.isNoteOn() && cell.duration > 0) {
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
      offCell->duration = cell.duration;
      offCell->link(pendingOff);
      pendingOff = offCell;
    } else {
      player(cell.event);
    }
  }

  AbsTime playPendingOff(AbsTime dt) {
    AbsTime nextT = forever;

    for (Cell *p = pendingOff, *q = nullptr; p;) {
      if (dt < p->duration) {
        p->duration -= dt;
        nextT = std::min(nextT, static_cast<AbsTime>(p->duration));
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
  : muted(false), volume(100),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0), length(0), position(0)
  { }

Layer::~Layer()
  { clear(); }

AbsTime Layer::next() const {
  if (!recentCell) return forever;
  if (recentCell->atEnd()) return forever;
  return recentCell->nextTime - timeSinceRecent;
}

AbsTime Layer::advance(AbsTime dt) {
  AbsTime nextT = forever;

  if (recentCell) {
    if (recentCell->atEnd()) {
      if (dt > maxEventInterval - timeSinceRecent) {
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
        timeSinceRecent = 0;
        recentCell = nextCell;
        playCell(*recentCell, muted, volume);
      }

      timeSinceRecent += dt;

      nextT = static_cast<AbsTime>(recentCell->nextTime - timeSinceRecent);
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
  newCell->duration = 0;

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
  timeSinceRecent = 0;
}

void Layer::keep(AbsTime baseLength) {
  if (!firstCell) return;

  int adj = 0;
  if (baseLength > 0)
    adj = syncLength(baseLength, length, timeSinceRecent);

  // closing the loop
  recentCell->link(firstCell);
  recentCell->nextTime = timeSinceRecent + adj;
  firstCell = nullptr;

  length += adj;
  position = position % length;

  // advance into the start of the loop
  advance(adj < 0 ? -adj : 0);
}

void Layer::clear() {
  Cell* start = recentCell;

  while (recentCell) {
    Cell* doomed = recentCell;
    recentCell = doomed->next();
    doomed->free();
    if (recentCell == start)
      break;
  }

  recentCell = nullptr;
  firstCell = nullptr;

  timeSinceRecent = 0;
  length = 0;
  position = 0;

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


AbsTime Loop::advance(AbsTime dt) {
  // In theory the offs should be interleaved as we go through the next
  // set of cells to play. BUT, since dt has already elapsed, it is roughly
  // okay to just spit out the NoteOff events first. And anyway, dt is rarely
  // more than 1.

  AbsTime nextT = playPendingOff(dt);

  for (auto& l : layers)
    nextT = std::min(nextT, l.next());

  while (dt > 0) {
    AbsTime et = std::min(dt, nextT);

    nextT = forever;
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

  activeLayer += activeLayer < (layers.size() - 1) ? 1 : 0;
  layerArmed = true;
  layerCount = std::max<uint8_t>(layerCount, activeLayer + 1);
}

void Loop::arm() {
  armed = true;
}

void Loop::clear() {

  clearAwatingOff();

  for (auto& l : layers)
    l.clear();

  armed = true;
  layerCount = 1;
  activeLayer = 0;
  layerArmed = true;
}


void Loop::layerMute(uint8_t layer, bool muted) {
  if (layer < layers.size()) layers[layer].muted = muted;
}

void Loop::layerVolume(uint8_t layer, uint8_t volume) {
  if (layer < layers.size()) layers[layer].volume = volume;
}

void Loop::layerArm(uint8_t layer) {
  if (layerArmed && activeLayer == layer && walltime < (armedTime + 1000)) {
    // double press of the layer arm control
    layers[activeLayer].clear();
    return;
  }

  // FIXME: what to do if still recording initial layer?
  activeLayer = layer;
  layerArmed = true;
  armedTime = walltime;

  layerCount = std::max<uint8_t>(layerCount, activeLayer + 1);
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
      sl.length = 0;
      sl.position = 0;
      sl.muted = false;
    }
  }

  return s;
}

void Loop::begin(EventFunc p) {
  player = p;
  Cell::begin();
}

AbsTime Loop::setTime(AbsTime now) {
  if (now < walltime) return 0;
    // FIXME: Handle rollover of walltime?
  AbsTime dt = now - walltime;
  walltime = now;
  return dt;
}

