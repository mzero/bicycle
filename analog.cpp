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


  inline void cvOut(int id, float v) {
    // input is assumed in [-1.0..1.0]

    constexpr float scale =  0.90;
    constexpr float offset = 0.02;
      // SAMD51 DAC outputs don't really go full scale, and snf asymmetrically.
      // This scales the outputs to a reaonsable range, determined by scope.
      // The same adjustment is used for the PWM outputs just to keep things
      // simple.

    uint16_t s = round((v * scale * -1 + offset + 1.0) * 32768);
    analogWrite(cvPins[id], s);
  }

  inline void trigOut(int id, bool v) {
    digitalWrite(trigPins[id], v ? LOW : HIGH);
  }

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

  constexpr float twoPi = 2 * PI;

  inline float waveform(float p) {
    // phase is in range [0.0..1.0)
    // produces a value in the range [-1.0..1.0]
    switch (testType) {
      case testSine:      return arm_sin_f32(p * twoPi);
      case testTriangle:  return 4 * (p < 0.5 ? p : (1 - p)) - 1;
      case testSaw:       return 2 * p - 1;
      default:
      case testSquare:    return p < 0.5 ? -1 : 1;
    }
  }

  void postInitTC(Tc* TCx) {
    TCx->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);
    // change the prescaler
    TCx->COUNT8.CTRLA.bit.PRESCALER = TC_CTRLA_PRESCALER_DIV1_Val;
    TCx->COUNT8.CTRLA.bit.ENABLE = 1;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);
  }
}



#if defined(__SAMD51__)

namespace {

  // set up TC3 to be sample interrupt for test waveforms
  // use timer in 8 bit mode, with a prescale of 1024

  constexpr float waveformDesiredSampleRate = 2000;  // Hz
  constexpr float waveformTimerClockRate = F_CPU / 1024;
  constexpr uint8_t waveformTimerPeriod =
    round(waveformTimerClockRate / waveformDesiredSampleRate);
  constexpr float waveformSampleRate =
    waveformTimerClockRate / waveformTimerPeriod;

  void setupWaveformTimer() {
    GCLK->PCHCTRL[TC3_GCLK_ID].reg =
      GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN_GCLK0;

    TC3->COUNT8.CTRLA.bit.SWRST = 1;
    while (TC3->COUNT8.SYNCBUSY.bit.SWRST);

    TC3->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TC3->COUNT8.SYNCBUSY.bit.ENABLE);

    TC3->COUNT8.CTRLA.reg = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024;

    while (TC3->COUNT8.SYNCBUSY.bit.CC0);

    TC3->COUNT8.PER.reg = waveformTimerPeriod;
    while (TC3->COUNT8.SYNCBUSY.bit.PER);

    TC3->COUNT8.CTRLA.bit.ENABLE = 1;

    NVIC_SetPriority(TC3_IRQn, 0);
    NVIC_EnableIRQ(TC3_IRQn);

  }

  void startWaveformTimer() {
    TC3->COUNT8.INTENSET.reg = TC_INTENSET_OVF;
  }

  void stopWaveformTimer() {
    TC3->COUNT8.INTENCLR.reg = TC_INTENCLR_OVF;
  }


  std::array<float, 4> phase = { 0.0, 0.0, 0.0, 0.0 };
  std::array<float, 4> delta = {
    10.0 /* Hz */ / waveformSampleRate,
    11.0 /* Hz */ / waveformSampleRate,
    12.0 /* Hz */ / waveformSampleRate,
    13.0 /* Hz */ / waveformSampleRate,
  };

  int tickCount = 0;

  void waveformTick() {
    ++tickCount;
    for (int i = 0; i < phase.size(); ++i) {
      auto p = phase[i] = fmod(phase[i] + delta[i], 1.0);

      cvOut(i, waveform(p));
      trigOut(i, p < 0.10);
    }
  }

}

void TC3_Handler() {
  if (TC3->COUNT8.INTFLAG.reg & TC_INTFLAG_OVF) {
    waveformTick();
    TC3->COUNT8.INTFLAG.reg = TC_INTFLAG_OVF;   // writing 1 clears the flag
  }
}
#endif



void analogBegin() {

  analogWriteResolution(16);

  for (auto& pin : cvPins)
    analogWrite(pin, 0);

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

  setupWaveformTimer();

  for (int i = 0; i < cvPins.size(); ++i)
    cvOut(i, 0.0);
  for (int i = 0; i < trigPins.size(); ++i)
    trigOut(i, false);
}

void analogUpdate(unsigned long now) {
  static unsigned long nextUpdate = 0;
  if (testType == testOff || now < nextUpdate) return;
  nextUpdate = now + 500;

  Serial.printf("tickCount: %d\n", tickCount);
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

  if (testType == testOff)    stopWaveformTimer();
  else                        startWaveformTimer();
}



