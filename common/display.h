#ifndef _INCLUDE_DISPLAY_H_
#define _INCLUDE_DISPLAY_H_

#include "looper.h"


void displaySetup();
void displayUpdate(unsigned long, const Loop::Status&);
void displayClear();

// define these to attach some functionality to the display's A, B, & C buttons
extern void buttonActionA();
extern void buttonActionB();
extern void buttonActionC();


#endif // _INCLUDE_DISPLAY_H_
