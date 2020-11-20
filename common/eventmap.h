#ifndef _INCLUDE_EVENTMAP_H_
#define _INCLUDE_EVENTMAP_H_

#include "types.h"

template<typename T>
class EventMap {
  public:
    typedef T                 value_t;
    typedef const MidiEvent&  key_t;
    typedef uint8_t           ch_t;
    typedef uint8_t           num_t;

    EventMap(const value_t& d) : defaultValue(d) {
      for (num_t n = 0; n < 128; ++n)
        for (ch_t c = 0; c < 16; ++c)
          values[n][c] = d;
    }

    bool has(key_t k) const               { return ref(k) == defaultValue; }
    const value_t& get(key_t k) const       { return ref(k); }
    void clear(key_t k)                   { ref(k) = defaultValue; }

    void set(key_t k, const value_t& v)   { ref(k) = v; }
    void setAllChannels(key_t k, const value_t& v) {
      const num_t n = k.data1 & 127;
      for (ch_t c = 0; c < 16; ++c)
        values[n][c] = v;
    }

    void set(ch_t c, num_t n, const value_t& v) { ref(c, n) = v; }
    void setAllChannels(num_t n, const value_t& v) {
      for (ch_t c = 0; c < 16; ++c)
        values[n][c] = v;
    }

  private:
    const value_t defaultValue;
    value_t values[128][16];

    value_t& ref(key_t k)
      { return values[k.data1 & 127][k.status & 0x0f]; }
    const value_t& ref(key_t k) const
      { return values[k.data1 & 127][k.status & 0x0f]; }

    value_t& ref(ch_t c, num_t n)
      { return values[n][c]; }
    const value_t& ref(ch_t c, num_t n) const
      { return values[n][c]; }
};

#endif // _INCLUDE_EVENTMAP_H_
