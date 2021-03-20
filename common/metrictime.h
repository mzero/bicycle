#ifndef _INCLUDE_METRICTIME_H_
#define _INCLUDE_METRICTIME_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <ratio>
#include <type_traits>

#include "types.h"


/***
 *** RATIOS
 ***/


using Pulses = std::ratio<1>;
  // All metrical timing is with respect to "the pulse".
  // For the most part, "beat" = "pulse", esp. in terms like "BPM".

using Spokes = std::ratio_divide<Pulses, std::ratio<16*3*5*7>>;
  /* Spokes are smallest unit of metrical time.

  A Spoke is 1/1680th of a Pulse

  at  30bpm, a Spoke is 1.19ms
  at 120bpm, a Spoke is 0.30ms
  at 300bpm, a Spoke is 0.12ms
  */


// Western music theory ratios of the pulse:
using QuarterNotes = Pulses;
using WholeNotes = std::ratio_multiply<Pulses, std::ratio<4>>;

// MIDI's ratios of the pulse
using MidiClocks = std::ratio_divide<QuarterNotes, std::ratio<24>>;
using MidiBeats = std::ratio_multiply<MidiClocks, std::ratio<4>>;


/***
 *** DURATION TEMPLATE
 ***/


template< class Rep >
struct MetricDuration {
public:
  using Units = Spokes;

  template< typename Rep2 >
  constexpr explicit MetricDuration(Rep2 r) : _count(r) { }

  template< typename Rep2 >
  constexpr MetricDuration(const MetricDuration<Rep2>& r)
    : _count(r.count()) { }

  constexpr Rep count() const { return _count; }

	constexpr MetricDuration() = default;
	MetricDuration(const MetricDuration&) = default;

  template< typename URatio >
  float inUnits() const {
    using Factor = std::ratio_divide<Spokes, URatio>;
    constexpr float factor =
      static_cast<float>(Factor::num) / static_cast<float>(Factor::den);
    return float(_count) * factor;
  }

  float inPulses() const { return inUnits<Pulses>(); }

  template< typename URatio >
  constexpr static MetricDuration fromUnits(int n) {
    using Factor = std::ratio_divide<URatio, Spokes>;
    return MetricDuration(n * Factor::num / Factor::den);
  }

  MetricDuration& operator+=(const MetricDuration& rhs) {
    _count += rhs._count;
    return *this;
  }

  MetricDuration& operator-=(const MetricDuration& rhs) {
    _count -= rhs._count;
    return *this;
  }

  MetricDuration operator-() const {
    return MetricDuration(-_count);
  }

  MetricDuration retime(double rate) {
    return MetricDuration(std::round(_count * rate));
  }

private:
  Rep _count;

public:
  static constexpr MetricDuration zero() noexcept
    { return MetricDuration(Rep(0)); }
  static constexpr MetricDuration min() noexcept
    { return MetricDuration(std::numeric_limits<Rep>::lowest()); }
  static constexpr MetricDuration max() noexcept
    { return MetricDuration(std::numeric_limits<Rep>::max()); }
};

// COMMON COMPARISON AND ARITHMETIC

template< class Rep1, class Rep2 >
constexpr bool operator==(const MetricDuration<Rep1>& lhs,
                          const MetricDuration<Rep2>& rhs)
{
  using RepC = std::common_type_t<Rep1, Rep2>;
  return RepC(lhs.count()) == RepC(rhs.count());
}
template< class Rep1, class Rep2 >
constexpr bool operator!=(const MetricDuration<Rep1>& lhs,
                         const MetricDuration<Rep2>& rhs)
{
  return !(lhs == rhs);
}

template< class Rep1, class Rep2 >
constexpr bool operator<(const MetricDuration<Rep1>& lhs,
                         const MetricDuration<Rep2>& rhs)
{
  using RepC = std::common_type_t<Rep1, Rep2>;
  return RepC(lhs.count()) < RepC(rhs.count());
}

template< class Rep1, class Rep2 >
constexpr bool operator>(const MetricDuration<Rep1>& lhs,
                         const MetricDuration<Rep2>& rhs)
{
  return rhs < lhs;
}

template< class Rep1, class Rep2 >
constexpr bool operator<=(const MetricDuration<Rep1>& lhs,
                          const MetricDuration<Rep2>& rhs)
{
  return !(rhs < lhs);
}

template< class Rep1, class Rep2 >
constexpr bool operator>=(const MetricDuration<Rep1>& lhs,
                          const MetricDuration<Rep2>& rhs)
{
  return !(lhs < rhs);
}

template< class Rep1, class Rep2 >
constexpr MetricDuration<std::common_type_t<Rep1, Rep2>>
operator+(const MetricDuration<Rep1>& lhs,
          const MetricDuration<Rep2>& rhs)
{
  using RepC = std::common_type_t<Rep1, Rep2>;
  return MetricDuration<RepC>(RepC(lhs.count()) + RepC(rhs.count()));
}

template< class Rep1, class Rep2 >
constexpr MetricDuration<std::common_type_t<Rep1, Rep2>>
operator-(const MetricDuration<Rep1>& lhs,
          const MetricDuration<Rep2>& rhs)
{
  using RepC = std::common_type_t<Rep1, Rep2>;
  return MetricDuration<RepC>(RepC(lhs.count()) - RepC(rhs.count()));
}

template< class Rep1, class Rep2 >
constexpr MetricDuration<std::common_type_t<Rep1, Rep2>>
operator%(const MetricDuration<Rep1>& lhs,
          const MetricDuration<Rep2>& rhs)
{
  using RepC = std::common_type_t<Rep1, Rep2>;
  return MetricDuration<RepC>(RepC(lhs.count()) % RepC(rhs.count()));
}



/***
 *** DURATION TYPES
 ***/


using NoteDuration = MetricDuration<uint16_t>;
  // always positive or zero, okay for the resolution to be low
  // maximum duration is ~39 Pulses

constexpr NoteDuration minDuration = NoteDuration(1);
constexpr NoteDuration maxDuration = NoteDuration::max();


using EventInterval = MetricDuration<int32_t>;
  // intervals between events in metrical time
  // absolute maximum is about 2.5M Pulses, or over 5 days even at 300bpm



/***
 *** TEMPO
 ***/

using BeatsPerMinute = std::ratio_divide<QuarterNotes, std::ratio<60>>;

class Tempo {
public:
	Tempo(const Tempo&) = default;

  template< class Rep, class FromUnits = BeatsPerMinute >
  explicit Tempo(Rep t) {
    using Conv = std::ratio_divide<FromUnits, Units>;
    rate = static_cast<double>(t) * Conv::num / Conv::den;
  }

  double inBPM() const {
    using Conv = std::ratio_divide<Units, BeatsPerMinute>;
    return rate * Conv::num / Conv::den;
  }

  TimeInterval toTimeInterval(EventInterval d) const {
    using Conv =
      std::ratio_divide<
        std::ratio_divide<EventInterval::Units, Units>,
        TimeInterval::period>;

    static_assert(Conv::num == 1);
    static_assert(Conv::den == 1);

    double count = d.count() / rate * Conv::num / Conv::den;
    return TimeInterval(static_cast<TimeInterval::rep>(std::round(count)));
  }

  EventInterval toEventInterval(TimeInterval t) const {
    using Conv =
      std::ratio_divide<
        std::ratio_multiply<TimeInterval::period, Units>,
        EventInterval::Units>;

    static_assert(Conv::num == 1);
    static_assert(Conv::den == 1);

    double count = t.count() * rate * Conv::num / Conv::den;
    return EventInterval(std::round(count));
  }

  static double retimeRate(const Tempo& from, const Tempo& to) {
    return to.rate / from.rate;
  }

private:
  using Units = std::ratio_divide<Spokes, std::micro>;
  double rate;
};


/***
 *** TIMING SPECIFICATION
 ***/

enum class TempoMode {
  begin = 0,
  inferred = begin,
  locked,
  synced,
  end
};

struct TimingSpec {
  Tempo         tempo;        // tempo currently playing
  Tempo         lowTempo;     // low range of tempo estimates
  Tempo         highTempo;    // high range of tempo estimates

  TempoMode     tempoMode;

  Meter         meter;        // meter currently playing
  bool          lockedMeter;  // layer timing will be locked to given meter

  TimingSpec()
    : tempo(120.0), lowTempo(75.0), highTempo(140.0),
      tempoMode(TempoMode::inferred),
      meter{4, 4},
      lockedMeter(false)
      { }

  EventInterval baseLength() const {
    using Factor = std::ratio_divide<WholeNotes, EventInterval::Units>;
    return EventInterval(meter.beats * Factor::num / meter.base / Factor::den);
  }
};


#endif // _INCLUDE_METRICTIME_H_
