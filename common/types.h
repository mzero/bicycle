#ifndef _INCLUDE_TYPES_H_
#define _INCLUDE_TYPES_H_

#include <cstdint>


typedef uint16_t DeltaTime;
typedef uint32_t AbsTime;

constexpr AbsTime forever = UINT32_MAX;


union MidiEvent {
  struct {
    uint8_t   status;
    uint8_t   data1;
    uint8_t   data2;
  };

  uint8_t bytes[3];

  bool isNoteOn() const
    { return (status & 0xf0) == 0x90 && data2 != 0; }

  bool isNoteOff() const
    { return (status & 0xf0) == 0x80 || ((status & 0xf0) == 0x90 && data2 == 0); }

  bool isCC() const
    { return (status & 0xf0) == 0xb0; }

};



#endif // _INCLUDE_TYPES_H_
