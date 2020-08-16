#include "display.h"

#include <ClearUI.h>


namespace {

  Loop::Status currentStatus;

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


  template< typename T >
  class TextField : public Field {
  public:
    typedef T value_t;

    TextField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : Field(x, y, w, h)
      {}

  protected:
    bool isOutOfDate() { return drawnValue != getValue(); }

    void redraw() {
      drawnValue = getValue();

      resetText();
      smallText();
      display.setCursor(x, y);
      drawValue(drawnValue);
    }

    virtual void drawValue(const value_t& v) const {
      display.print(v);
    }

    virtual value_t getValue() const = 0;

  private:
    value_t drawnValue;
  };


  class LengthField : public TextField<AbsTime> {
  public:
    LengthField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : TextField<AbsTime>(x, y, w, h) { }
  protected:
    void drawValue(const AbsTime& t) const {
      auto tenths = (t + 50) / 100;
      display.printf("%4d.%1d", tenths / 10, tenths % 10);
    }
    AbsTime getValue() const { return currentStatus.length; }
  };


  class LayerField : public Field {
  public:
    LayerField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : Field(x, y, w, h) { }
  protected:
    bool isOutOfDate() {
      return drawnLayerCount != currentStatus.layerCount
          || drawnActiveLayer != currentStatus.activeLayer
          || drawnLayerArmed != currentStatus.layerArmed
          || drawnLayerMutes != currentStatus.layerMutes;
    }
    void redraw() {
      drawnLayerCount = currentStatus.layerCount;
      drawnActiveLayer = currentStatus.activeLayer;
      drawnLayerArmed = currentStatus.layerArmed;
      drawnLayerMutes = currentStatus.layerMutes;

      auto c = foreColor();
      auto p = x;

      for (uint8_t i = 0; i < drawnLayerCount; ++i) {
        if (i >= drawnLayerMutes.size()) {
          display.writePixel(p    , y + 3, c);
          display.writePixel(p + 2, y + 3, c);
          display.writePixel(p + 4, y + 3, c);
          break;
        }
        if (i == drawnActiveLayer && drawnLayerArmed)
                                      display.drawRect(p, y, 4, 4, c);
        else if (drawnLayerMutes[i])  display.drawFastHLine(p, y + 3, 4, c);
        else                          display.fillRect(p, y, 4, 4, c);

        p += 5 + (i % 3 == 2 ? 2 : 0);
      }
    }

  private:
    uint8_t drawnLayerCount;
    uint8_t drawnActiveLayer;
    bool drawnLayerArmed;
    std::array<bool, 9> drawnLayerMutes;
  };



  auto loopField = LoopField(0, 0, 128, 13);
  auto lengthField = LengthField(92, 15, 28, 8);
  auto layerField = LayerField(20, 15, 80, 5);

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
    drew |= lengthField.render(force);
    drew |= layerField.render(force);

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
