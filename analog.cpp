#include "analog.h"

#include <array>
#include <cmath>

#include <Arduino.h>
#undef abs    // because wiring's abs is a horrible macro
#undef round  // because wiring's round is a horrible macro
#undef PI     // because arm_math.h and Arduino both define one

#include <arm_math.h>
#include <wiring_private.h>   // for pinPeripheral


// In this section of code, be very careful about numeric types
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wconversion"



/**
***  Control Voltage Outputs
**/

#if defined(__SAMD51__)

namespace {

  // use DACs on A0 and A1 for c.v. 0 and 1
  // use PWM on A4 and A5 for c.v. 2 and 3

  // The core's analogWrite system is used, but the PWM frequency is
  // increased by modifying the prescaler used.

  const std::array<uint32_t, numberOfCvOuts> cvPins =
    { PIN_A0, PIN_A1, PIN_A4, PIN_A5 };

  void postInitTC(Tc* TCx) {
    TCx->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);
    // change the prescaler
    TCx->COUNT8.CTRLA.bit.PRESCALER = TC_CTRLA_PRESCALER_DIV1_Val;
    TCx->COUNT8.CTRLA.bit.ENABLE = 1;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);
  }

  void setupCv() {
    analogWriteResolution(16);

    for (auto& pin : cvPins)
      analogWrite(pin, 0);
    postInitTC(TC0);
    postInitTC(TC1);
  }
}

void cvOut(int id, float v) {
  // input is assumed in [-1.0..1.0]

  constexpr float scale =  0.90;
  constexpr float offset = 0.02;
    // SAMD51 DAC outputs don't really go full scale, and snf asymmetrically.
    // This scales the outputs to a reaonsable range, determined by scope.
    // The same adjustment is used for the PWM outputs just to keep things
    // simple.

  v = clamp(v, -1.0, 1.0);
  uint16_t s = round((v * scale + offset + 1.0) * 32768);
  analogWrite(cvPins[id], s);
}

#endif



#if defined(__SAMD21G18A__)

  // use PWM on outputs A3, A4, 10, & 12

  // rather than use the core, this code sets up TCC0 to do the PWM
  // using the four compare channels, which mapped to those four pins.

namespace {

  constexpr float pwmDesiredSampleRate = 100000;  // Hz
  constexpr float pwmTimerClockRate = F_CPU / 1;
  constexpr float pwmTimerPeriod =
    round(pwmTimerClockRate / pwmDesiredSampleRate);

  inline void sync(Tcc* tcc, uint32_t mask) {
    while(tcc->SYNCBUSY.reg & mask);
  }

  void setupCv() {
    GCLK->CLKCTRL.reg
      = GCLK_CLKCTRL_CLKEN
      | GCLK_CLKCTRL_GEN_GCLK0
      | GCLK_CLKCTRL_ID(GCM_TCC0_TCC1);

    sync(TCC0, TCC_SYNCBUSY_SWRST);
    TCC0->CTRLA.bit.SWRST = 1;
    sync(TCC0, TCC_SYNCBUSY_SWRST);

    sync(TCC0, TCC_SYNCBUSY_WAVE);
    TCC0->WAVE.reg
      = TCC_WAVE_WAVEGEN_NPWM;
    sync(TCC0, TCC_SYNCBUSY_WAVE);

    sync(TCC0, TCC_SYNCBUSY_PER);
    TCC0->PER.reg = pwmTimerPeriod - 1;

    sync(TCC0, TCC_SYNCBUSY_ENABLE);
    TCC0->CTRLA.bit.ENABLE = 1;
    sync(TCC0, TCC_SYNCBUSY_ENABLE);

    // use the core to set up the i/o multiplexer
    pinPeripheral(A3, PIO_TIMER);       // WO[0], CC0
    pinPeripheral(A4, PIO_TIMER);       // WO[1], CC1
    pinPeripheral(10, PIO_TIMER_ALT);   // WO[2], CC2
    pinPeripheral(12, PIO_TIMER_ALT);   // WO[3], CC3
  }


  const std::array<int, numberOfCvOuts> cvChannels =
    { 0, 1, 3, 2 };
    // the last two were accidentially wired swapped to the outputs
}


void cvOut(int id, float v) {
  // input is assumed in [-1.0..1.0]

  constexpr float scale =  0.90f;
  constexpr float offset = 0.02f;
    // SAMD51 DAC outputs don't really go full scale, and snf asymmetrically.
    // This scales the outputs to a reaonsable range, determined by scope.
    // The same adjustment is used for the PWM outputs just to keep things
    // simple.

  uint32_t s = lround((v * scale + offset + 1.0f) * (pwmTimerPeriod / 2.0f));
  TCC0->CCB[cvChannels[id]].reg = s;
}


#endif


/**
***  Trigger Outputs
**/

namespace {

  const std::array<uint32_t, numberOfTrigOuts> trigPins =
    { PIN_SPI_MISO, PIN_SPI_SCK, PIN_SERIAL1_TX, PIN_SPI_MOSI };


  void setupTrig() {
    for (auto& pin : trigPins)
      pinMode(pin, OUTPUT);
  }
}

void trigOut(int id, bool v) {
  digitalWrite(trigPins[id], v ? HIGH : LOW);
}


/**
***  Test Waveforms
**/


namespace {

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

  constexpr float twoPi = 2.0f * PI;

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

}


#if defined(__SAMD51__) || defined(__SAMD21G18A__)

namespace {
  // set up TC3 to be sample interrupt for test waveforms
  // use timer in 8 bit mode, with a prescale of 1024

  constexpr float waveformDesiredSampleRate = 2000;  // Hz
  constexpr float waveformTimerClockRate = F_CPU / 256;
  constexpr uint8_t waveformTimerPeriod =
    round(waveformTimerClockRate / waveformDesiredSampleRate);
  constexpr float waveformSampleRate =
    waveformTimerClockRate / waveformTimerPeriod;

  void setupWaveformTimer() {
#if defined(__SAMD51__)
    GCLK->PCHCTRL[TC3_GCLK_ID].reg =
      GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN_GCLK0;

    TC3->COUNT8.CTRLA.bit.SWRST = 1;
    while (TC3->COUNT8.SYNCBUSY.bit.SWRST);

    TC3->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TC3->COUNT8.SYNCBUSY.bit.ENABLE);

    TC3->COUNT8.CTRLA.reg = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV256;

    while (TC3->COUNT8.SYNCBUSY.bit.CC0);

    TC3->COUNT8.PER.reg = waveformTimerPeriod;
    while (TC3->COUNT8.SYNCBUSY.bit.PER);

    TC3->COUNT8.CTRLA.bit.ENABLE = 1;
#endif
#if defined(__SAMD21G18A__)
    GCLK->CLKCTRL.reg
      = GCLK_CLKCTRL_CLKEN
      | GCLK_CLKCTRL_GEN_GCLK0
      | GCLK_CLKCTRL_ID(GCM_TCC2_TC3);

    TC3->COUNT8.CTRLA.reg = TC_CTRLA_SWRST;
    while(TC3->COUNT8.STATUS.bit.SYNCBUSY);

    TC3->COUNT8.CTRLA.reg = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV256;
    TC3->COUNT8.PER.reg = waveformTimerPeriod;

    while(TC3->COUNT8.STATUS.bit.SYNCBUSY);
    TC3->COUNT8.CTRLA.bit.ENABLE = 1;
    while(TC3->COUNT8.STATUS.bit.SYNCBUSY);
#endif

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
    10.0f /* Hz */ / waveformSampleRate,
    11.0f /* Hz */ / waveformSampleRate,
    12.0f /* Hz */ / waveformSampleRate,
    13.0f /* Hz */ / waveformSampleRate,
  };

  int tickCount = 0;

  void waveformTick() {
    ++tickCount;
    for (int i = 0; i < phase.size(); ++i) {
      auto p = phase[i] = fmodf(phase[i] + delta[i], 1.0);

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

#endif // defined(__SAMD51__) || defined(__SAMD21__)


#pragma GCC diagnostic pop



/**
***  Public Functions
**/

void analogBegin() {
  setupCv();
  setupTrig();

  for (int i = 0; i < numberOfCvOuts; ++i)
    cvOut(i, -1.0f);
  for (int i = 0; i < numberOfTrigOuts; ++i)
    trigOut(i, false);

  setupWaveformTimer();
}

void analogUpdate(unsigned long now) {
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



