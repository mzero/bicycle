#include "cell.h"


namespace {
  bool      storageInitialized = false;
  CellIndex freeIndex;
}

Cell Cell::storage[2000];


void Cell::begin() {
  if (storageInitialized) return;

  freeIndex = nullIndex;
  for (CellIndex i = 0; i < sizeof(storage)/sizeof(storage[0]); ++i) {
    storage[i].nextCell = freeIndex;
    freeIndex = i;
  }

  storageInitialized = true;
}


Cell* Cell::alloc() {
  if (freeIndex == nullIndex) return nullptr;

  Cell* c = &storage[freeIndex];
  freeIndex = c->nextCell;
  c->nextCell = nullIndex;
  return c;
}

void Cell::free() {
  this->nextCell = freeIndex;
  freeIndex = this - storage;
}


Cell* Cell::next() const {
  return nextCell == nullIndex ? nullptr : &storage[nextCell];
}

void Cell::link(Cell* newNext) {
  nextCell = newNext ? newNext - storage : nullIndex;
}
