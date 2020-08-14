#include "looper.h"

#include <cassert>
#include <cstring>

#include "cell.h"


Loop::Loop(EventFunc func)
  : player(func),
    armed(true), epoch(1),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0)
  { }


void Loop::advance(DeltaTime dt) {
  if (!recentCell) return;

  if (recentCell->atEnd()) {
    if (dt > maxEventInterval - timeSinceRecent) {
      clear();
      arm();
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
    } else {
      // prior data from this epoch, delete it
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
    epoch = 1;
  }

  Cell* newCell = Cell::alloc();
  if (!newCell) return; // ran out of cells!
  memcpy(newCell->event, ev, sizeof(ev));
  newCell->epoch = epoch;

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
  advance(0);
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

  firstCell = nullptr;
  recentCell = nullptr;
  timeSinceRecent = 0;
  armed = true;
  epoch = 1;
    // FIXME: do we need these variable clearings?
}

void Loop::begin() {
  Cell::begin();
}

