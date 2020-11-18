#ifndef __INCLUDE_MESSAGE_H_
#define __INCLUDE_MESSAGE_H_

#include <sstream>

class Log : public std::ostringstream {
  public:
    Log() : std::ostringstream() { }
    ~Log();

    static void begin();
    static void end();
};


class Message : public std::ostringstream {
  public:
    Message() : std::ostringstream() { }
    ~Message();

    static void good();
    static void bad();

    static const std::string& lastMessage();
};

#endif // __INCLUDE_MESSAGE_H_
