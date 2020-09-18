#ifndef __INCLUDE_MIDI_H__
#define __INCLUDE_MIDI_H__

#include "types.h"

class Midi {
  public:
    Midi() : impl(nullptr) { }
    ~Midi() { end(); }

    void begin();
    void end();

    bool send(const MidiEvent&);
    bool receive(MidiEvent&, AbsTime timeout);

  private:
    class Impl;
    Impl* impl;
};


#endif // __INCLUDE_MIDI_H__
