#include "Arduino.h"
#include "SPI.h"

#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <thread>


/**
 ** Time
 **/

void delay(uint32_t ms) {
  auto dur = std::chrono::milliseconds(ms);
  std::this_thread::sleep_for(dur);
}

uint32_t millis() {
  static const auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  const auto dur = now - start;
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
    // needs to be an explicit cast for some reason!
  return ms.count();
}


/**
 ** Pins
 **/

void pinMode(int, PinMode)        { }
void digitalWrite(int, PinState)  { }
PinState digitalRead(int)         { return LOW; }


uint32_t* portOutputRegister(uint32_t) {
  static uint32_t reg;
  return &reg;
}

uint32_t digitalPinToPort(int pin)    { return pin / 32; }
uint32_t digitalPinToBitMask(int pin) { return 1 << (pin % 32); }


/**
 ** Print & Serial
 **/

void Print::print(bool b)           { printf("%d", b ? 0 : 1); }
void Print::print(short int i)      { printf("%d", i); }
void Print::print(unsigned int u)   { printf("%u", u); }
void Print::print(char c)           { write(c); }
void Print::print(const char* s)    { printf("%s", s); }

void Print::println(bool b)           { printf("%d\n", b ? 0 : 1); }
void Print::println(short int i)      { printf("%d\n", i); }
void Print::println(unsigned int u)   { printf("%u\n", u); }
void Print::println(char c)           { write(c); write('\n'); }
void Print::println(const char* s)    { printf("%s\n", s); }

void Print::printf(const char* fmt, ...) {
  std::array<char, 1000> buf;
  
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf.data(), buf.size(), fmt, args);
  va_end(args);
  
  for (char c : buf) {
    if (c == 0) break;
    write(c);
  }
}

void Serial_t::begin(int) { }

size_t Serial_t::write(uint8_t c) {
  return putc(c, stdout) == EOF ? 0 : 1;
}

Serial_t Serial;


/**
 ** SPI - null implementation
 **/

void SPIClass::begin() { }
void SPIClass::transfer(uint8_t) { }

