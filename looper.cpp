#include "looper.h"

#include <cassert>
#include <cstring>

#include "cell.h"


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
};


Loop::Loop(EventFunc func)
  : player(func),
    walltime(0),
    armed(true), epoch(1),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0),
    pendingOff(nullptr)
  {
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
    walltime = now;

    if (dt > maxEventInterval - timeSinceRecent) {
      clear();
      return;
    }

    timeSinceRecent += dt;
    return;
  }

  while (recentCell->nextTime <= timeSinceRecent + dt) {
    // time to move to the next event, and play it

    Cell* nextCell = recentCell->next();
    if (nextCell->epoch != epoch) {
      // not from this epoch, so play it
      dt -= recentCell->nextTime - timeSinceRecent;
      timeSinceRecent = 0;

      recentCell = nextCell;
      recentCell->epoch = 0; // force to permanent epoch
      player(recentCell->event);

      if (recentCell->event.isNoteOn() && recentCell->duration > 0) {
        Cell* offCell = Cell::alloc();
        offCell->event.status = recentCell->event.status;
        offCell->event.data1 = recentCell->event.data1;
        offCell->event.data2 = 0;
        offCell->duration = recentCell->duration;
        offCell->link(pendingOff);
        pendingOff = offCell;
      }
    } else {
      // prior data from this epoch, delete it
      if (nextCell->event.isNoteOn())
        Util::cancelAwatingOff(*this, nextCell);

      recentCell->link(nextCell->next());
      recentCell->nextTime += nextCell->nextTime;
      nextCell->free();
    }
  }

  timeSinceRecent += dt;
}


void Loop::addEvent(const MidiEvent& ev) {
  if (armed) {
    clear();
    armed = false;
  }

  if (ev.isNoteOff()) {
    // note off processing
    Util::finishAwaitingOff(*this, ev);
    return;
  }

  Cell* newCell = Cell::alloc();
  if (!newCell) return; // ran out of cells!
  newCell->event = ev;
  newCell->epoch = epoch;
  newCell->duration = 0;

  if (ev.isNoteOn())
    Util::startAwaitingOff(*this, newCell);

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

  // ending the epoch
  epoch = epoch == 255 ? 1 : epoch + 1;

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
  armed = true;
  epoch = 1;
}

void Loop::begin() {
  Cell::begin();
}

