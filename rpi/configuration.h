#ifndef _INCLUDE_CONFIGURATION_H_
#define _INCLUDE_CONFIGURATION_H_

#include <vector>

#include "args.h"
#include "types.h"

enum class Action : char {
    none,
    ignore,

    arm,
    clear,
    keep,
    rearm,
    good,
    bad,

    layerArm,
    layerMute,
    layerVolume,

    tempoLow,
    tempoHigh,
    tempo,
    meterBeats,
    meterBase
};

struct Command {
    Action action;
    char layer;

    Command() : action(Action::none), layer(0) { }
    Command(Action a) : action(a), layer(0) { }
    Command(Action a, int l) : action(a), layer(l) { }

    bool operator<(const Command& rhs) const;
};

class Configuration {
public:
    static bool begin();
    static Command command(const MidiEvent&);

    static const std::vector<MidiEvent>& triggers(Command);
};

#endif // _INCLUDE_CONFIGURATION_H_