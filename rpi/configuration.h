#ifndef _INCLUDE_CONFIGURATION_H_
#define _INCLUDE_CONFIGURATION_H_

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
    layerVolume
};

struct Command {
    Action action;
    char layer;

    Command() : action(Action::none), layer(0) { }
    Command(Action a) : action(a), layer(0) { }
    Command(Action a, int l) : action(a), layer(l) { }
};

class Configuration {
public:
    static bool begin();
    static Command command(const MidiEvent&);
};

#endif // _INCLUDE_CONFIGURATION_H_