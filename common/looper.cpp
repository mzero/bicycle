#include "looper.h"

#include <algorithm>
#include <cassert>
#include <cstring>

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
}

class Loop::Util {
public:
  static void startAwaitingOff(Loop& loop, Cell* cell) {
    finishAwaitingOff(loop, cell->event);
    auto& ao = loop.awaitingOff[cell->event.data1];
    ao.cell = cell;
    ao.start = loop.walltime;
  }

  static void cancelAwatingOff(Loop& loop, const Cell* cell) {
    auto& ao = loop.awaitingOff[cell->event.data1];
    if (ao.cell == cell) {
      ao.cell = nullptr;
    }
  }

  static void finishAwaitingOff(Loop& loop, const MidiEvent& ev) {
    auto& ao = loop.awaitingOff[ev.data1];
    if (ao.cell) {
      ao.cell->duration = loop.walltime - ao.start;
      ao.cell = nullptr;
    }
  }

  static void clearAwatingOff(Loop& loop) {
    for (auto& ao : loop.awaitingOff)
      ao = {nullptr, 0};
  }


  static void playCell(Loop& loop, const Cell& cell) {
    auto layer = cell.layer;
    if (layer < loop.layerMutes.size() && loop.layerMutes[layer])
      return;

    if (cell.event.isNoteOn() && cell.duration > 0) {
      MidiEvent note = cell.event;
      if (layer < loop.layerVolumes.size())
        note.data2 = scaleVelocity(note.data2, loop.layerVolumes[layer]);
      if (note.data2 == 0)
        return;

      Cell* offCell = Cell::alloc();
      if (!offCell)
        return;   // don't play NoteOn if can't allocate NoteOff

      loop.player(note);

      offCell->event = note;
      offCell->event.data2 = 0; // volume 0 makes it a NoteOff
      offCell->duration = cell.duration;
      offCell->link(loop.pendingOff);
      loop.pendingOff = offCell;
    } else {
      loop.player(cell.event);
    }
  }
};


Loop::Loop(EventFunc func)
  : player(func),
    walltime(0),
    armed(true), layerCount(1), activeLayer(0), layerArmed(false),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0), length(0), position(0),
    pendingOff(nullptr)
  {
    for (auto& m : layerMutes) m = false;
    for (auto& v : layerVolumes) v = 100;

    Util::clearAwatingOff(*this);
  }


void Loop::advance(AbsTime now) {
  // In theory the offs should be interleaved as we go through the next
  // set of cells to play. BUT, since dt has already elapsed, it is roughly
  // okay to just spit out the NoteOff events first. And anyway, dt is rarely
  // more than 1.

  AbsTime dt = now - walltime;
  walltime = now;
    // FIXME: Handle rollover of walltime?

  for (Cell *p = pendingOff, *q = nullptr; p;) {
    if (dt < p->duration) {
      p->duration -= dt;
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

  if (!recentCell) return;

  if (recentCell->atEnd()) {
    if (dt > maxEventInterval - timeSinceRecent) {
      clear();
      return;
    }

    timeSinceRecent += dt;
    length += dt;
    position += dt;
    return;
  }
  position = (position + dt) % length;

  while (recentCell->nextTime <= timeSinceRecent + dt) {
    // time to move to the next event, and play it

    Cell* nextCell = recentCell->next();
    auto layer = nextCell->layer;

    if (layer == activeLayer && !layerArmed) {
      // prior data from this layer currently recording into, delete it
      // note: if the layer is armed, then awaiting first event to start
      // recording
      if (nextCell->event.isNoteOn())
        Util::cancelAwatingOff(*this, nextCell);

      recentCell->link(nextCell->next());
      recentCell->nextTime += nextCell->nextTime;
      nextCell->free();
    } else {
      dt -= recentCell->nextTime - timeSinceRecent;
      timeSinceRecent = 0;
      recentCell = nextCell;
      Util::playCell(*this, *recentCell);
    }
  }

  timeSinceRecent += dt;
}


void Loop::addEvent(const MidiEvent& ev) {
  if (ev.isNoteOff()) {
    // note off processing
    player(ev);
    Util::finishAwaitingOff(*this, ev);
    return;
  }

  if (armed) {
    clear();
    armed = false;
  }
  if (layerArmed) {
    layerArmed = false;
  }

  if (activeLayer < layerMutes.size())
    layerMutes[activeLayer] = false;
    // TODO: Should we be doing this? how to communicate back to controller?

  if (ev.isNoteOn()) {
    MidiEvent note = ev;
    if (activeLayer < layerVolumes.size())
      note.data2 = scaleVelocity(note.data2, layerVolumes[activeLayer]);
    player(note);
  } else {
    player(ev);
  }

  Cell* newCell = Cell::alloc();
  if (!newCell) return; // ran out of cells!
  newCell->event = ev;
  newCell->layer = activeLayer;
  newCell->duration = 0;

  if (ev.isNoteOn())
    Util::startAwaitingOff(*this, newCell);

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


void Loop::keep() {
  if (firstCell) {
    // closing the loop
    recentCell->link(firstCell);
    recentCell->nextTime = timeSinceRecent;
    firstCell = nullptr;
  }

  activeLayer += activeLayer < (layerMutes.size() - 1) ? 1 : 0;
  layerArmed = true;
  layerCount = std::max<uint8_t>(layerCount, activeLayer + 1);

  // advance into the start of the loop
  advance(walltime);
}

void Loop::arm() {
  armed = true;
}

void Loop::clear() {
  Cell* start = recentCell;

  while (recentCell) {
    Cell* doomed = recentCell;
    recentCell = doomed->next();
    doomed->free();
    if (recentCell == start)
      break;
  }

  Util::clearAwatingOff(*this);

  firstCell = nullptr;
  recentCell = nullptr;
  timeSinceRecent = 0;
  length = 0;
  position = 0;
  armed = true;
  layerCount = 1;
  activeLayer = 0;
  layerArmed = true;
  for (auto& m : layerMutes) m = false;
    // TODO: Should we be doing this? how to communicate back to controller?
}


void Loop::layerMute(uint8_t layer, bool muted) {
  if (layer < layerMutes.size()) layerMutes[layer] = muted;
}

void Loop::layerVolume(uint8_t layer, uint8_t volume) {
  if (layer < layerVolumes.size()) layerVolumes[layer] = volume;
}

void Loop::layerArm(uint8_t layer) {
  if (layerArmed && activeLayer == layer && walltime < (armedTime + 1000)) {
    // if a duouble press of the layer arm control, start recording
    layerArmed = false;
    return;
  }

  // FIXME: what to do if still recording initial layer?
  activeLayer = layer;
  layerArmed = true;
  armedTime = walltime;

  layerCount = std::max<uint8_t>(layerCount, activeLayer + 1);
}

Loop::Status Loop::status() const {
  Status s;
  s.length = length;
  s.position = position;
  s.layerCount = layerCount;
  s.activeLayer = activeLayer;
  s.looping = !firstCell;
  s.armed = armed;
  s.layerArmed = layerArmed;
  s.layerMutes = layerMutes;
  return s;
}

void Loop::begin() {
  Cell::begin();
}

