#ifndef _INCLUDE_CLOCK_H_
#define _INCLUDE_CLOCK_H_

#include "metrictime.h"


class ClockLayer {
public:
  ClockLayer();

  void set(EventInterval length_);
  void clear();

  EventInterval next() const;
  EventInterval advance(EventInterval dt);

private:
  bool running;

  EventInterval length;
  EventInterval position;
  EventInterval nextClock;

public:
  static void setPlayer(EventFunc);
};

extern ClockLayer clockLayer;


#endif // _INCLUDE_CLOCK_H_
