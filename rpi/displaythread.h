#ifndef _INCLUDE_DISPLAYTHREAD_H_
#define _INCLUDE_DISPLAYTHREAD_H_

#include "looper.h"
#include "types.h"

namespace DisplayThread {
  void start();
  void end();

  bool readyForUpdate();
  void update(const Loop::Status&);
}

#endif // _INCLUDE_DISPLAYTHREAD_H_
