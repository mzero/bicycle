#ifndef _INCLUDE_CELL_H_
#define _INCLUDE_CELL_H_

#include "types.h"

typedef uint16_t CellIndex;
const CellIndex nullIndex = 0xffff;


struct Cell {
public:
  MidiEvent   event;
  DeltaTime   duration;
  DeltaTime   nextTime;

private:
  CellIndex   nextCell;

public:
  static Cell* alloc();
  void free();

  bool atEnd() const { return nextCell == nullIndex; }

  Cell* next() const;
  void link(Cell*);

  static void begin();
  static int inUse();
  
private:
  Cell() { };

  static Cell storage[2000];
};

#endif // _INCLUDE_CELL_H_
