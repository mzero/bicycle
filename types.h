#ifndef _INCLUDE_TYPES_H_
#define _INCLUDE_TYPES_H_

#include <cstdint>


typedef uint16_t DeltaTime;

struct MidiEvent {
  uint8_t   status;
  uint8_t   data1;
  uint8_t   data2;
};



#endif // _INCLUDE_TYPES_H_
