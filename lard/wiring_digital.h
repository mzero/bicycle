#ifndef __INCLUDE_LARD_WIRING_DIGITAL_H__
#define __INCLUDE_LARD_WIRING_DIGITAL_H__

#include <cstdint>


void pinMode(int, PinMode);
void digitalWrite(int, PinState);
PinState digitalRead(int);

uint32_t* portOutputRegister(uint32_t);
uint32_t digitalPinToPort(int);
uint32_t digitalPinToBitMask(int);


#endif // __INCLUDE_LARD_WIRING_DIGITAL_H__
