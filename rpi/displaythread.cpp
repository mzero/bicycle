#include "displaythread.h"

#include <chrono>
#include <thread>

#include "display.h"

namespace {

  std::thread displayThread;

  // This is a bit of cheap hack... but std::semaphore isn't until C++20!

  // set in display thread, cleared in main thread
  volatile bool waitingForUpdate = false;

  // set in main thread, cleared in display thread
  volatile bool timeToEnd = false;
  volatile bool updateAvalable = false;

  Loop::Status status;


  void displayLoop() {
    static const auto clockStart = std::chrono::steady_clock::now();

    displaySetup();
    waitingForUpdate = true;

    while (true) {
      if (timeToEnd) break;

      if (updateAvalable) {
        // static auto displayTime = std::chrono::steady_clock::duration::zero();
        // static int displayCount = 0;
        // auto dTimeStart = std::chrono::steady_clock::now();

        const auto sinceStart = std::chrono::steady_clock::now() - clockStart;
        auto millisSinceStart =
          std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart).count();

        displayUpdate(millisSinceStart, status);
        updateAvalable = false;

        // displayTime += (std::chrono::steady_clock::now() - dTimeStart);
        // displayCount += 1;
        // if (displayCount == 100) {
        //   float dTimeAvgMs =
        //     std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(displayTime).count() / displayCount;
        //   std::cout << "average display time = " << dTimeAvgMs << "ms\n";

        //   displayTime = std::chrono::steady_clock::duration::zero();
        //   displayCount = 0;
        // }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waitingForUpdate = true;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    displayClear();
  }
}


namespace DisplayThread {
  void start() {
    displayThread = std::thread(displayLoop);
  }

  void end() {
    timeToEnd = true;
    if (displayThread.joinable())
      displayThread.join();
  }

  bool readyForUpdate() {
    return waitingForUpdate;
  }
  void update(const Loop::Status& s) {
    if (waitingForUpdate) {
      waitingForUpdate = false;
      status = s;
      updateAvalable = true;
    }
  }
}




/*
  static auto nextDisplayUpdate =
    std::chrono::steady_clock::duration::zero();

  if (sinceStart >= nextDisplayUpdate) {
    static auto displayTime = std::chrono::steady_clock::duration::zero();
    static int displayCount = 0;

    auto dTimeStart = std::chrono::steady_clock::now();

    auto millisSinceStart =
      std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart).count();

    Loop::Status s = theLoop.status();
    displayUpdate(millisSinceStart, s);

    displayTime += (std::chrono::steady_clock::now() - dTimeStart);
    displayCount += 1;
    if (displayCount == 100) {
      float dTimeAvgMs =
        std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(displayTime).count() / displayCount;
      std::cout << "average display time = " << dTimeAvgMs << "ms\n";

      displayTime = std::chrono::steady_clock::duration::zero();
      displayCount = 0;
    }

    const static auto displayRefresh = std::chrono::milliseconds(200);
      // The display taks about 18ms to update. If we update too often, then
      // MIDI responsiveness will suffer. If too slow, the display is choppy.

    nextDisplayUpdate = sinceStart + displayRefresh;
    return; // go 'round the loop again
*/