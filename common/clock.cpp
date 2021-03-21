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

  EventFunc playerFunc = nullptr;

  inline void player(const MidiEvent& ev) {
    if (playerFunc) playerFunc(ev);
  }
}

ClockLayer::ClockLayer() : running(false) { }

void ClockLayer::syncStart(EventInterval length_) {
  length = length_;
  position = EventInterval::zero();
  nextClock = EventInterval::zero();

  running = true;
  player(startEvent);
}

void ClockLayer::stop() {
  running = false;
  player(stopEvent);
}

void ClockLayer::resize(EventInterval length_) {
  length = length_;

  if (length != forever<EventInterval>)
    position = position % length;
}

EventInterval ClockLayer::next() const {
  return running ? nextClock : forever<EventInterval>;
}

EventInterval ClockLayer::advance(EventInterval dt) {
  if (!running) return forever<EventInterval>;

  while (nextClock <= dt) {
    dt -= nextClock;
    position + nextClock;

    player(clockEvent);

    nextClock = clockInterval;

  }
  nextClock -= dt;
  position += dt;

  if (length != forever<EventInterval>)
    position = position % length;

  return nextClock;
}

void ClockLayer::setPlayer(EventFunc p) {
  playerFunc = p;
}
