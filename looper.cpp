#include "looper.h"

#include <cassert>
#include <cstring>


namespace {

  typedef uint16_t CellIndex;
  const CellIndex nullIndex = 0xffff;

}


struct Loop::Cell {
public:
  uint8_t epoch;
  MidiEvent event;
  DeltaTime   nextTime;

private:
  CellIndex   nextCell;

public:
  static Cell* alloc();
  void free();

  Cell* next() const;
  bool atEnd() const;
  void link(Cell*);

  static void begin();

private:
  Cell() { };

  static Cell storage[2000];
  static bool storageInitialized;
  static CellIndex freeIndex;
};

Loop::Cell  Loop::Cell::storage[2000];
bool        Loop::Cell::storageInitialized = false;
CellIndex   Loop::Cell::freeIndex;


Loop::Cell* Loop::Cell::alloc() {
  if (freeIndex == nullIndex) return nullptr;

  Cell* c = &storage[freeIndex];
  freeIndex = c->nextCell;
  c->nextCell = nullIndex;
  return c;
}

void Loop::Cell::free() {
  this->nextCell = freeIndex;
  freeIndex = this - storage;
}

inline Loop::Cell* Loop::Cell::next() const {
  return nextCell == nullIndex ? nullptr : &storage[nextCell];
}

inline bool Loop::Cell::atEnd() const {
  return nextCell == nullIndex;
}

inline void Loop::Cell::link(Cell* newNext) {
  nextCell = newNext ? newNext - storage : nullIndex;
}

void Loop::Cell::begin() {
  assert(sizeof(Cell) == 8);

  if (storageInitialized) return;

  freeIndex = nullIndex;
  for (CellIndex i = 0; i < sizeof(storage)/sizeof(storage[0]); ++i) {
    storage[i].nextCell = freeIndex;
    freeIndex = i;
  }

  storageInitialized = true;
}


Loop::Loop()
  : armed(true), epoch(1),
    firstCell(nullptr), recentCell(nullptr),
    timeSinceRecent(0)
  { }


void Loop::advance(DeltaTime dt, EventFunc f) {
  if (!recentCell) return;

  if (recentCell->atEnd()) {
    // FIXME: handle overflow
    timeSinceRecent += dt;
    return;
  }

  while (dt) {
    DeltaTime now = timeSinceRecent + dt;
    if (now < recentCell->nextTime) {
      timeSinceRecent = now;
      return;
    }

    // advance to next event
    dt = now - recentCell->nextTime;
    timeSinceRecent = 0;
    recentCell = recentCell->next();
    if (recentCell->epoch == epoch) {
      // FIXME: delete this cell....
    } else {
      f(recentCell->event);
    }
  }
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
    recentCell = firstCell;
    timeSinceRecent = 0;
    firstCell = nullptr;
    // FIXME: Need to play the first event here
  }

  // ending the epoch
  epoch = epoch == 255 ? 1 : epoch + 1;
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

