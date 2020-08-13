#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "looper.h"

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;


Loop theLoop;

void playEvent(const MidiEvent& ev) {
  uint8_t packet[4];

  packet[0] = 0; // FIXME
  packet[1] = ev[0];
  packet[2] = ev[1];
  packet[3] = ev[2];

  usb_midi.send(packet);
  Serial.printf("usb_midi.sending %02x %02x %02x %02x\n",
    packet[0], packet[1], packet[2], packet[3]);
}

void noteEvent(const MidiEvent& ev) {
  switch (ev[0] & 0xf0) {
    case 0xb0: // CC
      switch (ev[1]) {
        case 44:  theLoop.arm();    return;
        case 46:  theLoop.clear();  return;
        case 49:  theLoop.keep();   return;
      }
  }

  theLoop.addEvent(ev);
}

void notePacket(const uint8_t packet[4]) {
  MidiEvent ev;
  ev[0] = packet[1];
  ev[1] = packet[2];
  ev[3] = packet[3];
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
    theLoop.advance(now - then, playEvent);
    then = now;
  }

  uint8_t packet[4];
  if (usb_midi.receive(packet)) {

    notePacket(packet);
    Serial.printf("usb_midi.receive'd %02x %02x %02x %02x\n",
      packet[0], packet[1], packet[2], packet[3]);
  }
}


