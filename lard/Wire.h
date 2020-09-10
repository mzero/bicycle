#ifndef __INCLUDE_LARD_WIRE_H__
#define __INCLUDE_LARD_WIRE_H__

#include <array>
#include <cstdint>


class TwoWire {
  public:
    TwoWire();
    ~TwoWire();

    void begin();
    void end();
    
    void beginTransmission(int8_t);
    void endTransmission();
    
    void write(uint8_t);

  private:
    int fd;

    std::array<uint8_t,256> tx_buf;
    uint8_t *tx_next;
};

extern TwoWire Wire;



#endif // __INCLUDE_LARD_WIRE_H__
