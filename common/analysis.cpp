#include "analysis.h"

#include <cmath>

#include "cell.h"
#include "message.h"
#include "types.h"


EventInterval syncLength(
    EventInterval base, EventInterval len, EventInterval maxShorten) {
  const int limit = 7;

  if (base == EventInterval::zero())
    return EventInterval::zero();

  Log log;
  log << "layer sync: "
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
    Tempo recTempo, EventInterval recLength, const Cell* firstCell) {
  constexpr float minBPM = 75; // 85;
  constexpr float maxBPM = 140; // 2 * minBPM;

  using Seconds = std::chrono::duration<float>;
  constexpr Seconds maxQnote = std::chrono::minutes(1) / minBPM;
  constexpr Seconds minQnote = std::chrono::minutes(1) / maxBPM;

  Seconds basef(recTempo.toTimeInterval(recLength));

  int minMN = int(ceilf(basef / maxQnote));
  int maxMN = int(floorf(basef / minQnote));

  int bestMN = 0;
  float bestErr = INFINITY;

  for (int mn = minMN; mn <= maxMN; ++mn) {
    const Seconds qn = basef / mn;
    const float qnf = qn.count();

    float err = 0.0f;
    auto p = firstCell;
    EventInterval t(0);
    while (p) {
      Seconds ts(recTempo.toTimeInterval(t));
      float tf = ts.count();
      err += beatError(tf / qnf);
      if (err > bestErr) break;

      t += p->nextTime;
      p = p->next();
      if (p == firstCell) break;
    }

    if (err < bestErr) {
      bestMN = mn;
      bestErr = err;
    }
  }

  {
    Log log;
    log << "layer deltas: ";
    auto p = firstCell;
    while (p) {
      log << p->nextTime.count() << ",";
      p = p->next();
      if (p == firstCell) break;
    }
  }

  if (bestMN == 0)
    bestMN = 1; // should never happen, but just to be safe

  float bestBPM = std::chrono::minutes(1) / (basef / bestMN);

  {
    Log log;
    log << "meter: " << bestMN << "beats ("
      << minMN << "," << maxMN << ") @ " << bestBPM << " bpm";

    Message msg;
    msg << bestMN << " @ " << static_cast<int>(bestBPM) << " bpm";
  }

  return { Tempo(bestBPM), { bestMN, 4 } };
}
