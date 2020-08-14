#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "looper.h"

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;




void playEvent(const MidiEvent& ev) {
  uint8_t packet[4];

  packet[0] = 0 | (ev[0] >> 4);
    // TODO: This works because of the limited range of things the
    // noteEvent() accepts into the looper... but this should be
    // fixed support all messages just to be safe.
  packet[1] = ev[0];
  packet[2] = ev[1];
  packet[3] = ev[2];

  usb_midi.send(packet);
}

Loop theLoop(playEvent);



void noteEvent(const MidiEvent& ev) {
  switch (ev[0] & 0xf0) {
    case 0x80: // Note Off
    case 0x90: // Note On
    case 0xa0: // Poly Aftertouch
      break;

    case 0xb0: // CC
      switch (ev[1]) {
        case 44:  if (ev[2]) theLoop.arm();    return;
        case 46:  if (ev[2]) theLoop.clear();  return;
        case 49:  if (ev[2]) theLoop.keep();   return;
      }
      break;

    case 0xc0: // Program change
      return;     // TODO: echo these?

    case 0xd0: // Channel Aftertouch
    case 0xe0: // Pitch Bend
      break;

    case 0xf0: // System Messages
      return;

    default:
      return;
  }

  playEvent(ev);
  theLoop.addEvent(ev);
}

void notePacket(const uint8_t packet[4]) {
  MidiEvent ev;
  ev[0] = packet[1];
  ev[1] = packet[2];
  ev[2] = packet[3];
  noteEvent(ev);
}



void setup() {
  Serial.begin(115200);
  while (!Serial);

  theLoop.begin();

  usb_midi.begin();
  while (!USBDevice.mounted()) delay(1);

  Serial.println("Ready!");
}

void loop() {
  static uint32_t then = millis();
  uint32_t now = millis();

  if (now > then) {
    theLoop.advance(now - then);
    then = now;
  }

  uint8_t packet[4];
  if (usb_midi.receive(packet)) {

    notePacket(packet);
  }
}


