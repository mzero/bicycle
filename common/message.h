#ifndef __INCLUDE_MESSAGE_H_
#define __INCLUDE_MESSAGE_H_

#include <sstream>

class Log : public std::ostringstream {
  public:
    Log() : std::ostringstream() { }
    ~Log();

    typedef void (*PersistFunc)(const std::string&);
    static void begin(PersistFunc = nullptr);
    static void end();
};


class Message : public std::ostringstream {
  public:
    Message() : std::ostringstream() { }
    ~Message();

    static void clear();
    static void good();
    static void bad();

    static const std::string& lastMessage();
};

#endif // __INCLUDE_MESSAGE_H_
