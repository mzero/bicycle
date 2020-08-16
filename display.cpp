#include "display.h"

#include <ClearUI.h>


namespace {

  Loop::Status currentStatus;
    // this is a total, ugly hack!
    // the fields should have access to the Loop object and request
    // the values they need.....


  class LoopField : public Field {
  public:
    LoopField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : Field(x, y, w, h)
      {}

  protected:
    virtual bool isOutOfDate() {
      return drawnLooping != currentStatus.looping
          || drawnMarkerX != markerX();
    }

    virtual void redraw() {
      drawnLooping = currentStatus.looping;
      drawnMarkerX = markerX();

      uint16_t c = foreColor();
      uint16_t ymid = y + h/2;

      // FIXME: Make these bitmaps
      display.drawFastVLine(x + 0, y, h, c);
      display.drawFastVLine(x + 1, y, h, c);
      display.drawFastVLine(x + 3, y, h, c);
      display.fillRect(x + 5, ymid - 3, 2, 2, c);
      display.fillRect(x + 5, ymid + 2, 2, 2, c);

      if (drawnLooping) {
        display.drawFastVLine(x + w - 1, y, h, c);
        display.drawFastVLine(x + w - 2, y, h, c);
        display.drawFastVLine(x + w - 4, y, h, c);
        display.fillRect(x + w - 7, ymid - 3, 2, 2, c);
        display.fillRect(x + w - 7, ymid + 2, 2, 2, c);
      }

      display.drawFastHLine(x, ymid, w, c);

      display.drawFastVLine(x + 8 + drawnMarkerX, ymid - 3, 7, c);
    }

  private:
    bool      drawnLooping;
    uint16_t  drawnMarkerX;

    uint16_t markerX() {
      uint32_t l = w - 16 - 1;
      if (currentStatus.length == 0) return 0;

      return currentStatus.position * l / currentStatus.length;
    }
  };


  auto loopField = LoopField(0, 0, 128, 13);
  //auto mainPage = Layout({&loopField}, 0);

  // class MainPage : public Layout {
  // public:
  //   MainPage()
  //     : Layout({}, 0),
  //       loopField(0, 0, 128, 20)
  //     { }
  //   bool render(bool refresh);
  // private:
  //   LoopField loopField;
  // };

  // bool MainPage::render(bool refresh) {
  //   bool updated = Layout::render(refresh);
  //   updated |= loopField.render(refresh);
  //   return updated;
  // }


  // MainPage mainPage;

  bool drawAll(bool force) {
    bool drew = force;

    if (force) {
      display.clearDisplay();
      resetText();
      smallText();
    }

    drew |= loopField.render(force);

    if (drew)
      display.display();

    return drew;
  }



  // IdleTimeout selectionTimeout(2000);
  IdleTimeout dimTimeout(5000);
}


void displaySetup() {
  initializeDisplay();


  // updateSaver(true);
  dimTimeout.activity();

}

void displayUpdate(unsigned long now, bool activity, const Loop::Status& s) {

  static unsigned long nextDraw = 0;
  static bool saverDrawn = false;

  if (activity) {
    display.dim(false);
    dimTimeout.activity();
  }

  bool drew = false;
  if (activity || now > nextDraw) {
    currentStatus = s;

    drew = drawAll(true);
    nextDraw = now + 50;  // redraw 20x a second

    if (drew && saverDrawn)
        drawAll(true);    // need to redraw if the saver had been drawn
  }

  if (dimTimeout.update()) {
    display.dim(true);
  }

  saverDrawn = updateSaver(drew);
}
