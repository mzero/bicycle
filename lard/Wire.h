#ifndef __INCLUDE_LARD_WIRE_H__
#define __INCLUDE_LARD_WIRE_H__

#include <cstdint>


class TwoWire {
  public:
    void begin();
    
    void beginTransmission(int8_t);
    void endTransmission();
    
    void write(uint8_t);
};

extern TwoWire Wire;



#endif // __INCLUDE_LARD_WIRE_H__
