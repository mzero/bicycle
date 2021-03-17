#ifndef _INCLUDE_ANALYSIS_H_
#define _INCLUDE_ANALYSIS_H_

#include "cell.h"
#include "metrictime.h"

EventInterval syncLength(
    EventInterval base, EventInterval len, EventInterval maxShorten);


struct TimeSignature {
  Tempo tempo;
  Meter meter;
};


TimeSignature estimateTimeSignature(
    Tempo recTempo, EventInterval recLength, const Cell* firstCell);


#endif // _INCLUDE_ANALYSIS_H_
