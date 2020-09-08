#ifndef __INCLUDE_LARD_WIRING_CONSTANTS_H__
#define __INCLUDE_LARD_WIRING_CONSTANTS_H__

enum PinMode : int {
  INPUT = 0,
  OUTPUT = 1,
  INPUT_PULLUP = 2,
  INPUT_PULLDOWN = 3,
};

enum PinState : int {
  LOW = 0,
  HIGH = 1,
};

#endif // __INCLUDE_LARD_WIRING_CONSTANTS_H__
