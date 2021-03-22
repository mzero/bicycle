#include "analysis.h"

#include <cmath>
#include <iomanip>

#include "cell.h"
#include "message.h"
#include "metrictime.h"


EventInterval syncLength(
    EventInterval base, EventInterval len, EventInterval maxShorten) {
  const int limit = 7;

  if (base == EventInterval::zero())
    return EventInterval::zero();

  Log log;
  log << "syncLength: "
    << base.count() << " :: " << len.count()
    << " (" << maxShorten.count() << ") = ";

  double b = base.count();
  double l = len.count();
  double x = 0.0 - maxShorten.count();

  int nBest = 0;
  int mBest = 0;
  double errBest = INFINITY;

  for (int i = 1; i < limit; ++i) {
    int n;
    int m;

    if (b < l) {
      n = std::lround(i * l / b);
      m = i;
    } else {
      n = i;
      m = std::lround(i * b / l);
    }
    if (i > 1 && (n >= limit || m >= limit)) continue;

    double err = (n * b - m * l) / m;
    if (err > x && std::abs(err) < std::abs(errBest)) {
      nBest = n;
      mBest = m;
      errBest = err;
    }
  }
  Message msg;
  if (nBest == 0) {
    log << "failed to find relationship";
    msg << "?:?";
    return EventInterval::zero();
  }

  int adj = lround(errBest);
  log << nBest << ":" << mBest << ", adjusting by " << adj;
  msg << nBest << ":" << mBest;
  return EventInterval(adj);
}

float beatError(float x) {
  float e = 0.0f;
  for (int i = 0; i < 8; ++i) {
    x = x - std::trunc(x);
    e += x >= 0.5f ? (1.0f - x) : x;
    x = x + x;
  }
  return e;
}


TimeSignature estimateTimeSignature(
    const TimingSpec& spec, EventInterval recLength, const Cell* firstCell) {

  Log log;
  log << "estimateTimeSignature:\n";
  log << std::setprecision(2) << std::fixed;

  Meter meter = spec.meter;
  if (!spec.lockedMeter) meter.beats = 1;

  EventInterval baseUnit =
    EventInterval::fromUnits<WholeNotes>(1) * meter.beats / meter.base;

  using Seconds = std::chrono::duration<float>;
  Seconds maxBaseSec = spec.lowTempo.toTimeInterval(baseUnit);
  Seconds minBaseSec = spec.highTempo.toTimeInterval(baseUnit);

  Seconds phraseSec(spec.tempo.toTimeInterval(recLength));

  int minN = int(ceilf(phraseSec / maxBaseSec));
  int maxN = int(floorf(phraseSec / minBaseSec));


  log << "     meter            " << meter.beats << '/' << meter.base << '\n';
  log << "     baseUnit count   " << baseUnit.count() << '\n';
  log << "     baseSec range    " << minBaseSec.count()
                                  << '~' << maxBaseSec.count() << '\n';
  log << "     phraseSec        " << phraseSec.count() << '\n';
  log << "     range of N       " << minN << '~' << maxN << '\n';

  int bestN = 0;
  float bestErr = INFINITY;

  for (int n = minN; n <= maxN; ++n) {
    const Seconds qn = phraseSec / n;
    const float qnf = qn.count();

    float err = 0.0f;
    auto p = firstCell;
    EventInterval t(0);
    while (p) {
      Seconds ts(spec.tempo.toTimeInterval(t));
      float tf = ts.count();
      err += beatError(tf / qnf);
      if (err > bestErr) break;

      t += p->nextTime;
      p = p->next();
      if (p == firstCell) break;
    }

    if (err < bestErr) {
      bestN = n;
      bestErr = err;
    }
  }

  {
    log << "     layer deltas     ";
    auto p = firstCell;
    EventInterval t(0);
    while (p) {
      log << p->nextTime.count() << ",";
      t += p->nextTime;
      p = p->next();
      if (p == firstCell) break;
    }
    log << '\n';
    log << "     layer total      " << t.count() << '\n';
  }

  if (bestN == 0)
    bestN = 1; // should never happen, but just to be safe

  float baseBeats = baseUnit.inUnits<QuarterNotes>();
  float phrasePerMinute = std::chrono::minutes(1) / phraseSec;
  float phraseBPM = phrasePerMinute * bestN * baseBeats;

  log << std::setw(8);
  log << "     baseBeats        " << baseBeats << '\n';
  log << "     phrasePerMinute  " << phrasePerMinute << '\n';
  log << "     phraseBPM        " << phraseBPM << '\n';

  Message msg;

  if (meter.beats == 1) {
    log << "     meter: "
        << bestN << '/' << meter.base << '\n';
    msg << bestN << '/' << meter.base;
  }
  else {
    log << "     meter: "
        << bestN << 'x' << meter.beats << '/' << meter.base << '\n';
    msg << bestN << 'x' << meter.beats << '/' << meter.base;
  }

  return { Tempo::fromBPM(phraseBPM), { bestN * meter.beats, meter.base } };
}
