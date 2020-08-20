#include "analog.h"

#include <array>

#include <arm_math.h>
#undef PI  // because arm_math.h and Arduino both define one

#include <Arduino.h>


namespace {

  const std::array<uint32_t, 4> cvPins =
    { PIN_A0, PIN_A1, PIN_A4, PIN_A5 };

  const std::array<uint32_t, 4> trigPins =
    { PIN_SPI_MISO, PIN_SPI_SCK, PIN_SERIAL1_TX, PIN_SPI_MOSI };

  enum TestWave {
    testBegin,

    testOff = testBegin,
    testSine,
    testTriangle,
    testSaw,
    testSquare,

    testEnd
  };

  TestWave testType = testOff;
  uint32_t testPin = 0;

  const uint32_t sampleLo  = 0x0001;
  const uint32_t sampleMid = 0x8000;
  const uint32_t sampleHi  = 0xffff;

  void postInitTC(Tc* TCx) {
    TCx->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);
    // change the prescaler
    TCx->COUNT8.CTRLA.bit.PRESCALER = TC_CTRLA_PRESCALER_DIV1_Val;
    TCx->COUNT8.CTRLA.bit.ENABLE = 1;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);
  }
}


void analogBegin() {

  analogWriteResolution(16);

  for (auto& pin : cvPins)
    analogWrite(pin, sampleMid);

  //   if (pin == PIN_DAC0 || pin == PIN_DAC1) {
  //     // DACs
  //   } else {
  //     // PWM
  //     PinDescription pinDesc = g_APinDescription[pin];
  //     Tc* TCx = (Tc*) GetTC(pinDesc.ulPWMChannel);
  //     TCx->COUNT8.CTRLA.bit.PRESCALER = TC_CTRLA_PRESCALER_DIV1_Val;
  //   }
  // }

  postInitTC(TC0);
  postInitTC(TC1);


  for (auto& pin : trigPins)
    digitalWrite(pin, HIGH);

}

void analogUpdate(unsigned long now) {
  constexpr unsigned long period = 1000 / 1;  // ms/s / Hz

  auto phase = now % period;
  auto t = (phase * 0x7ffful + period / 2) / period % 0x8000ul;
    // [0..0x8000)

  uint32_t s = sampleMid;

  switch (testType) {
    case testSine: {
      q15_t y = arm_sin_q15(static_cast<q15_t>(t));
      s = static_cast<uint32_t>(y) + 0x8000;
      break;
    }

    case testTriangle:
      s = t < 0x4000 ? 4 * t : 0x1ffff - 4 * t;
      break;

    case testSaw:
      s = t * 2;
      break;

    case testSquare:
      s = t < 0x4000 ? sampleLo : sampleHi;
      break;

    case testOff:
    default:
      return;
  }

  // static unsigned long nextPrint = 0;
  // if (now > nextPrint) {
  //   Serial.printf("sample @ %8d: phase %8x, t %8x, s %8x\n",
  //     now, phase, t, s);
  //   nextPrint = now + 50;
  // }

  constexpr uint32_t pctScale = 90;
  constexpr uint32_t pctOffset = 2;
  constexpr uint32_t adjMid =   (sampleMid * (100 - pctScale) + 50)/100;
  constexpr uint32_t adjOffset = (sampleHi * pctOffset + 50)/100;
  s = (s * pctScale + 50)/100 + adjMid + adjOffset;

  analogWrite(cvPins[testPin], s);
}


void toggleTestWave() {
  switch (testType) {
    case testOff:
      testType = testSine;
      Serial.println("test output waveform sine");
      break;
    case testSine:
      testType = testTriangle;
      Serial.println("test output waveform triangle");
      break;
    case testTriangle:
      testType = testSaw;
      Serial.println("test output waveform saw");
      break;
    case testSaw:
      testType = testSquare;
      Serial.println("test output waveform square");
      break;
    case testSquare:
    default:
      testType = testOff;
      Serial.println("test output waveform off");
      break;
  }
}

void toggleTestOutput() {
  if (++testPin == cvPins.size())
    testPin = 0;

  Serial.printf("test output on c.v. output %d (pin %d)\n",
      testPin, cvPins[testPin]);
}

