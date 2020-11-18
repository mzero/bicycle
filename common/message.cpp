#include "message.h"

#include <iostream>

#include "version.h"


namespace {
  Log::PersistFunc persistFunc = nullptr;

  void log(const std::string& msg) {
    if (msg.empty()) return;

    std::cout << msg;
    if (msg.back() != '\n') std::cout << '\n';

    if (persistFunc) persistFunc(msg);
  }
}

void Log::begin(PersistFunc pf) {
  persistFunc = pf;
  log("--- " BUILD_COMMIT " on " BUILD_DATE);
    // horrible hack of literal string concatenation
}

void Log::end() {
}

Log::~Log() {
  log(this->str());
}


namespace {
  std::string lastMsg;

}

Message::~Message() {
  lastMsg = this->str();
}


void Message::good() {
  log("good");
  if (!lastMsg.empty()) lastMsg.front() = 'Y';
}

void Message::bad() {
  log("bad");
  if (!lastMsg.empty()) lastMsg.front() = 'N';
}

const std::string& Message::lastMessage() {
  return lastMsg;
}
