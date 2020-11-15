#ifndef _INCLUDE_TYPES_H_
#define _INCLUDE_TYPES_H_

#include <chrono>
#include <cstdint>
#include <ratio>


// TIME

using NoteDuration = std::chrono::duration<uint16_t, std::milli>;
  // always positive or zero, okay for the resolution to be low
  // maximum duration is about 65 seconds

using TimeInterval = std::chrono::duration<int32_t, std::micro>;
  // interval between events
  // absolute maximum is about 35 minutes

using WallTime = std::chrono::microseconds;
  // covers well over 100 years... enough?


constexpr NoteDuration minDuration = NoteDuration(1);
constexpr NoteDuration maxDuration = NoteDuration::max();

constexpr TimeInterval forever = TimeInterval::max();


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

};



#endif // _INCLUDE_TYPES_H_
