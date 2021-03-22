#ifndef _INCLUDE_TYPES_H_
#define _INCLUDE_TYPES_H_

#include <chrono>
#include <cstdint>
#include <ratio>


// TIME

using TimeInterval = std::chrono::duration<int32_t, std::micro>;
  // interval between events
  // absolute maximum is about 35 minutes

using WallTime = std::chrono::microseconds;
  // covers well over 100 years... enough?



template< typename T >
constexpr T forever = T::max();

// MUSIC

struct Meter {
  int beats = 0;
  int base = 4;

  bool unspecified() const { return beats == 0; }
  bool specified() const { return beats != 0; }

  bool operator==(const Meter& rhs) const
    { return this->beats == rhs.beats && this->base == rhs.base; }
  bool operator!=(const Meter& rhs) const
    { return !(*this == rhs); }
};


// MIDI

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

  MidiEvent() { }
  MidiEvent(uint8_t s)                         : status(s), data1( 0), data2( 0) { }
  MidiEvent(uint8_t s, uint8_t d1)             : status(s), data1(d1), data2( 0) { }
  MidiEvent(uint8_t s, uint8_t d1, uint8_t d2) : status(s), data1(d1), data2(d2) { }
};


typedef void (*EventFunc)(const MidiEvent&);



#endif // _INCLUDE_TYPES_H_
