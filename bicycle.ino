#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "looper.h"
#include "types.h"


// USB MIDI object
Adafruit_USBD_MIDI usb_midi;


enum Pin : uint32_t {
  pinSequence = PIN_SPI_MOSI,
  pinMeasure = PIN_SERIAL1_TX,
  pinBeat = PIN_SPI_SCK,
  pinTuplet = PIN_SPI_MISO,
};

enum PinNote : uint8_t {
  pinNoteSequence = 50,
  pinNoteMeasure = 51,
  pinNoteBeat = 38,
  pinNoteTuplet = 36,
};

void playTrigger(uint8_t note, uint32_t level) {
  switch (note) {
    case pinNoteSequence:   digitalWrite(pinSequence, level);   break;
    case pinNoteMeasure:    digitalWrite(pinMeasure,  level);   break;
    case pinNoteBeat:       digitalWrite(pinBeat,     level);   break;
    case pinNoteTuplet:     digitalWrite(pinTuplet,   level);   break;
    default: ;
  }
}

void playEvent(const MidiEvent& ev) {
  uint8_t packet[4];

  if (ev.isNoteOff())       playTrigger(ev.data1, HIGH);
  else if (ev.isNoteOn())   playTrigger(ev.data1, LOW);


  packet[0] = 0 | (ev.status >> 4);
    // TODO: This works because of the limited range of things the
    // noteEvent() accepts into the looper... but this should be
    // fixed support all messages just to be safe.
  packet[1] = ev.status;
  packet[2] = ev.data1;
  packet[3] = ev.data2;

  usb_midi.send(packet);
}

Loop theLoop(playEvent);



void noteEvent(const MidiEvent& ev) {
  switch (ev.status & 0xf0) {
    case 0x80: // Note Off
    case 0x90: // Note On
    case 0xa0: // Poly Aftertouch
      break;

    case 0xb0: // CC
      switch (ev.data1) {
        case 44:  if (ev.data2) theLoop.arm();    return;
        case 46:  if (ev.data2) theLoop.clear();  return;
        case 49:  if (ev.data2) theLoop.keep();   return;
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
  ev.status = packet[1];
  ev.data1 = packet[2];
  ev.data2 = packet[3];
  noteEvent(ev);
}



void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(pinSequence, OUTPUT);
  pinMode(pinMeasure, OUTPUT);
  pinMode(pinBeat, OUTPUT);
  pinMode(pinTuplet, OUTPUT);
  digitalWrite(pinSequence, HIGH);
  digitalWrite(pinMeasure, HIGH);
  digitalWrite(pinBeat, HIGH);
  digitalWrite(pinTuplet, HIGH);

  theLoop.begin();

  usb_midi.begin();
  while (!USBDevice.mounted()) delay(1);

  Serial.println("Ready!");
}

void loop() {
  static uint32_t then = millis();
  uint32_t now = millis();

  if (now > then) {
    theLoop.advance(now);
    then = now;
  }

  uint8_t packet[4];
  while (usb_midi.receive(packet)) {
    notePacket(packet);
  }
}


