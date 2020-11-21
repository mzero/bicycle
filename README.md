= bicycle - a MIDI looper

**bicycle** is a MIDI looper embracing several ideas I've always wanted in a
live performance looper:

- layers can have different, but related loop times
- decide what to keep after it is played
- automatic beats & clock detection
- layers remain independent, mutable, rerecordable

The project is a very early work in progress... though it is good enough that
I have performed with it several times.

== Building

The project can be built two ways:
1) On a Raspberry Pi
   This uses an OLED bonnet for the display, and integrated with the
   ALSA MIDI Seq. subsystem for MIDI routing.

2) On an Arduino compatible SAMD micro controller
   I've been using an Adafruit Featuer M4 and OLED featherwing.
   The device acts as a MIDI USB device, so you need a USB host to route
   MIDI in and out of it.
   This version can optionally convert MIDI to C.V. and triggers.

== Credits & Thanks

=== Open source code used:

https://github.com/CLIUtils/CLI11
CLI11 1.8 Copyright (c) 2017-2019 University of Cincinnati, developed by Henry
Schreiner under NSF AWARD 1414736. All rights reserved.

