
# Machine Setup

**System:** This should work with pretty much any Raspberry Pi 2 or later.

**Display:** Adafruit 128x64 OLED Bonnet for Raspberry Pi
  - https://www.adafruit.com/product/3531
  - The looper will run without this being attached and none of the UI
    requires that you see the display. But I'm developing with the assumption
    it is there.

**Extra:** Pisound
  - https://blokas.io/pisound/
  - Not really needed, but I use it to get DIN MIDI in/out.

**MIDI devices**
  - Connect USB MIDI devices right into the Pi!
  - Use Pisound, or a USB audio/midi interface for DIN MIDI


## Operating System

In theory, any Linux based OS will work. I routinely develop with:

  * [Patchbox OS](https://blokas.io/patchbox-os/) — a simple, "just works" system set up for audio and MIDI
  * [Raspberry Pi OS](https://www.raspberrypi.org/software/)* — previously
  * The older **Raspbian** OS

## I2C

For the display, you'll need to enable i2c in raspi-config:

    sudo raspi-config

Navigate the menus to Interface > I2C, and then select Yes. Then exit.

You may want to also speed up the I2C bus. If the display is the only thing on it, it will almost certainly work at 10x the default speed - which will make display updates much smoother.

Edit the file `/boot/config.txt` and find the lines that start with `dtparam=`.
Add a line that reads:

    dtparam=i2c_baudrate=1000000

Now reboot.




# Build

You need to install a system dev package:

    sudo apt install libasound2-dev

One day I'll get this all properly set up with subrepos, but until then
you need this directory structure:

    projects/      -- can be anything you like
      amidiminder/ -- cloned from https://github.com/mzero/amidiminder.git
      bicycle/     -- cloned from https://github.com/mzero/bicycle.git
      ClearUI/     -- cloned from https://github.com/mzero/ClearUI.git
      ext/
        Adafruit-GFX-Library/ -- cloned from https://github.com/adafruit/Adafruit-GFX-Library.git
        Adafruit_SSD1306/     -- cloned from https://github.com/adafruit/Adafruit_SSD1306.git

You could set this up with:

    #!/bin/sh

    mkdir projects
    cd projects
    git clone https://github.com/mzero/amidiminder.git
    git clone https://github.com/mzero/bicycle.git
    git clone https://github.com/mzero/ClearUI.git
    mkdir ext
    cd ext
    git clone https://github.com/adafruit/Adafruit-GFX-Library.git
    git clone https://github.com/adafruit/Adafruit_SSD1306.git
    cd ..


# Build

You don't need to have `amidiminder`, but it'll keep you from having to type
`aconnect` all the time to connect your MIDI devices to `bicycle`.

    cd amidiminder
    make
    sudo dpkg -i build/amidiminder.deb

Building bicyle is easy:

    cd bicycle
    git checkout rpi
    make


# Run

    build/bicycle &

Now hook your MIDI stuff up. For example:

    $ aconnect -l
    client 0: 'System' [type=kernel]
        0 'Timer           '
        1 'Announce        '
    client 14: 'Midi Through' [type=kernel]
        0 'Midi Through Port-0'
    client 20: 'nanoKONTROL' [type=kernel,card=1]
        0 'nanoKONTROL MIDI 1'
    client 24: 'pisound' [type=kernel,card=2]
        0 'pisound MIDI PS-3DJNWEF'
    client 28: 'Circuit' [type=kernel,card=3]
        0 'Circuit MIDI 1  '
    client 32: 'nanoKEY2' [type=kernel,card=4]
        0 'nanoKEY2 MIDI 1 '
    client 128: 'bicycle' [type=user,pid=23920]
        0 'controllers     '
        1 'synths          '

    $ aconnect nanoKONTROL:0 bicycle:0

    $ aconnect nanoKEY2:0 bicycle:0

    $ aconnect bicycle:1 Circuit:0

# Configuration

See the file `bicycle.config`. The format should be pretty self explanitory.

