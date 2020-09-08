#ifndef __INCLUDE_LARD_ARDUINO_H__
#define __INCLUDE_LARD_ARDUINO_H__

#include <algorithm>
#include <cstring>

#include <Binary.h>
#include <delay.h>
#include <Print.h>
#include <wiring_constants.h>
#include <wiring_digital.h>
#include <WString.h>

#define PROGMEM


using std::max;
using std::min;

template<class T>
const T& constrain(const T& v, const T& lo, const T& hi)
{
    return min(max(v, lo), hi);
}

using std::memset;
using std::strncpy;

constexpr float PI = 3.1415926535;



class Serial_t : public Print {
  public:
    void begin(int);
};
extern Serial_t Serial;



char* utoa(unsigned int, char *, size_t);


#endif // __INCLUDE_LARD_ARDUINO_H__
