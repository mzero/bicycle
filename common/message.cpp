#include "message.h"

#include <iostream>

namespace {
  void log(const std::string& msg) {
    if (msg.empty()) return;

    std::cout << msg;
    if (msg.back() != '\n') std::cout << '\n';
  }

}

void Log::begin() {
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
