#include "display.h"

#include <ClearUI.h>


namespace {

  template< typename T >
  inline T roundingDivide(T x, T q) {
    return (x + q / 2) / q;
  }

  Loop::Status currentStatus;

  class LoopField : public Field {
  public:
    LoopField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : Field(x, y, w, h)
      {}

  protected:
    virtual bool isOutOfDate() {
      return true; // FIXME drawnLooping != currentStatus.looping;
    }

    virtual void redraw() {
      uint16_t c = foreColor();

      AbsTime spanT = 0;
      AbsTime keyPos = 0;
      int keyLayer = -1;
      for (int i = 0; i < currentStatus.layerCount; ++i) {
        auto& l = currentStatus.layers[i];
        if (l.length > spanT) {
          spanT = l.length;
          keyPos = l.position;
          keyLayer = i;
        }
      }

      if (spanT == 0) return;

      uint16_t px = 0;

      if (keyLayer >= 0) {
        auto& l = currentStatus.layers[keyLayer];
        int16_t ly = y + 4*keyLayer;

        display.drawFastHLine(x, ly+1, w, c);
        display.drawFastVLine(x, ly, 3, c);
        display.drawFastVLine(x+w-1, ly, 3, c);

        px = x + roundingDivide(l.position * w, spanT);

        display.drawFastVLine(px, ly, 3, c);
      }
      for (int i = 0; i < currentStatus.layerCount; ++i) {
        if (i == keyLayer) continue;

        auto& l = currentStatus.layers[i];
        if (l.length == 0) continue;

        int ls = static_cast<int>(keyPos) - static_cast<int>(l.position);
        int ll = static_cast<int>(l.length);

        int16_t ly = y + 4*i;
        int16_t lx = x + roundingDivide(ls * w, static_cast<int>(spanT));
        int16_t lw = roundingDivide(ll * w, static_cast<int>(spanT));

        display.drawFastHLine(lx, ly+1, lw, c);
        display.drawFastVLine(lx, ly, 3, c);
        display.drawFastVLine(lx+lw-1, ly, 3, c);
        display.drawFastVLine(px, ly, 3, c);
      }
    }

  private:
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
      display.printf("%2d.%1ds", tenths / 10, tenths % 10);
    }
    AbsTime getValue() const {
      AbsTime spanT = 0;
      for (int i = 0; i < currentStatus.layerCount; ++i)
        spanT = std::max(spanT, currentStatus.layers[i].length);
      return spanT;
    }
  };


  class LayerField : public Field {
  public:
    LayerField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : Field(x, y, w, h) { }
  protected:
    bool isOutOfDate() {
      return true; // FIXME
    }
    void redraw() {
      auto c = foreColor();
      auto p = x;

      for (uint8_t i = 0; i < currentStatus.layerCount; ++i) {
        if (i == currentStatus.activeLayer && currentStatus.layerArmed)
                                      display.drawRect(p, y, 4, 4, c);
        else if (currentStatus.layers[i].muted)
                                      display.drawFastHLine(p, y + 3, 4, c);
        else                          display.fillRect(p, y, 4, 4, c);

        p += 5 + (i % 3 == 2 ? 3 : 0);
      }
    }

  private:
    uint8_t drawnLayerCount;
    uint8_t drawnActiveLayer;
    bool drawnLayerArmed;
    std::array<bool, 9> drawnLayerMutes;
  };


  class ArmedField : public TextField<bool> {
  public:
    ArmedField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : TextField<bool>(x, y, w, h) { }
  protected:
    void drawValue(const bool& armed) const {
      if (armed)
        display.print("\xe0");
    }
    bool getValue() const { return currentStatus.armed; }
  };


  auto loopField = LoopField(0, 0, 128, 30);
  auto lengthField = LengthField(92, 34, 36, 8);
  auto layerField = LayerField(20, 37, 70, 5);
  auto armedField = ArmedField(0, 34, 10, 20);

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
    drew |= armedField.render(force);

    if (drew)
      display.display();

    return drew;
  }



  // IdleTimeout selectionTimeout(2000);
  IdleTimeout dimTimeout(5000);
}


void displaySetup() {
  initializeDisplay();
  display.setRotation(0);
  display.clearDisplay();
  display.display();


  // updateSaver(true);
  dimTimeout.activity();

}


void displayClear() {
  display.setRotation(0);
  display.clearDisplay();
  display.display();
}


namespace {
  Encoder encoder(12, 11);
  Button encoderButton(10);

  Button oledButtonA(9);
  Button oledButtonB(6);
  Button oledButtonC(5);

}

extern "C" {
  void buttonActionNOP() { }
};
void buttonActionA() __attribute__ ((weak, alias("buttonActionNOP")));
void buttonActionB() __attribute__ ((weak, alias("buttonActionNOP")));
void buttonActionC() __attribute__ ((weak, alias("buttonActionNOP")));


void displayUpdate(unsigned long now, const Loop::Status& s) {

  bool active = false;

  auto update = encoder.update();
  if (update.active()) {
    // updateSelection(update);
    active = true;
  }
  Button::State b = encoderButton.update();
  if (b != Button::NoChange) {
    // clickSelection(s);
    active = true;
  }
  if (oledButtonA.update() == Button::Down) {
    buttonActionA();
    active = true;
  }
  if (oledButtonB.update() == Button::Down) {
    buttonActionB();
    active = true;
  }
  if (oledButtonC.update() == Button::Down) {
    buttonActionC();
    active = true;
  }


  static unsigned long nextDraw = 0;
  static bool saverDrawn = false;

  if (active) {
    display.dim(false);
    dimTimeout.activity();
  }

  bool drew = false;
  if (active || now > nextDraw) {
    currentStatus = s;

    drew = drawAll(false);
    nextDraw = now + 50;  // redraw 20x a second

    if (drew && saverDrawn)
        drawAll(true);    // need to redraw if the saver had been drawn
  }

  if (dimTimeout.update()) {
    display.dim(true);
  }

  saverDrawn = updateSaver(drew);
}
