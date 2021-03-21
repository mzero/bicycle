#include "clock.h"

#include <algorithm>

#include "types.h"


ClockLayer clockLayer;

namespace {
  const MidiEvent clockEvent(0xf8);
  const MidiEvent startEvent(0xfa);
  const MidiEvent continueEvent(0xfb);
  const MidiEvent stopEvent(0xfc);

  constexpr const EventInterval clockInterval = EventInterval::fromUnits<MidiClocks>(1);
  static_assert(clockInterval.count() == 70,
    "checking MidiClock interval in Spokes");

  EventFunc player = nullptr;

  inline void play(const MidiEvent& ev) {
    if (player) player(ev);
  }
}

ClockLayer::ClockLayer() : running(false) { }

void ClockLayer::set(EventInterval length_) {
  length = length_;
  position = EventInterval::zero();
  nextClock = EventInterval::zero();

  running = true;
}

void ClockLayer::clear() {
  running = false;
  player(stopEvent);
}

EventInterval ClockLayer::next() const {
  return running ? nextClock : forever<EventInterval>;
}

EventInterval ClockLayer::advance(EventInterval dt) {
  if (!running) return forever<EventInterval>;

  while (nextClock <= dt) {
    dt -= nextClock;
    position += nextClock;

    if (position >= length) {
      player(stopEvent);
      player(startEvent);
      position -= length;
    }
    player(clockEvent);

    nextClock = std::min(clockInterval, length - position);

  }
  nextClock -= dt;
  position += dt;  // no need to % length, can't roll here!
  return nextClock;
}

void ClockLayer::setPlayer(EventFunc p) {
  player = p;
}
