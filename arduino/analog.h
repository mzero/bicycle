#ifndef _INCLUDE_ANALOG_H_
#define _INCLUDE_ANALOG_H_

void analogBegin();
void analogUpdate(unsigned long);

const int numberOfCvOuts = 4;
const int numberOfTrigOuts = 4;

void cvOut(int, float);
  // outputs are A0, A1, A4, A5

void trigOut(int, bool);
  // outputs are MISO, SCK, TX, MOSI
  // which are also known as Tuplet, Beat, Measure, Sequence

void toggleTestWave();

#endif // _INCLUDE_ANALOG_H_
