#include "display.h"

#include <cmath>
#include <string>

#include <ClearUI.h>

#include "cell.h"
#include "message.h"

namespace {

  template< typename T >
  inline T roundingDivide(T x, T q) {
    return (x + q / 2) / q;
  }

  void drawDottedHLine(int16_t x, int16_t y, int16_t w, uint16_t c) {
    for (auto i = x; i < x+w; i += 2)
      display.drawPixel(i, y, c);
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

      EventInterval spanT(0);
      EventInterval keyPos(0);
      int keyLayer = -1;
      for (int i = 0; i < currentStatus.layerCount; ++i) {
        auto& l = currentStatus.layers[i];
        if (l.length > spanT) {
          spanT = l.length;
          keyPos = l.position;
          keyLayer = i;
        }
      }

      if (spanT == EventInterval::zero()) return;
      const float scale = float(w) / spanT.count();

      auto mapT = [&](EventInterval t){
        return int16_t(std::round(t.count() * scale));
      };

      uint16_t px = 0;
      uint16_t ly = y;

      if (keyLayer >= 0) {
        auto& l = currentStatus.layers[keyLayer];
        px = x + mapT(l.position);
      }

      for (int i = 0; i < currentStatus.layerCount; ++i) {
        auto& l = currentStatus.layers[i];
        if (l.length != EventInterval::zero()) {
          EventInterval ls = keyPos - l.position;
          EventInterval ll = l.length;

          int16_t lx = x + mapT(ls);
          int16_t lw = mapT(ll);

          if (l.muted)
            drawDottedHLine(lx, ly+1, lw, c);
          else
            display.drawFastHLine(lx, ly+1, lw, c);
          display.drawFastVLine(lx, ly, 3, c);
          display.drawFastVLine(lx+lw-1, ly, 3, c);
          display.drawFastVLine(px, ly, 3, c);
        }

        ly += (i % 3 == 2) ? 7 : 4;
      }
    }

  private:
  };


  template< typename T >
  class ValueField : public Field {
  public:
    typedef T value_t;

    ValueField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : Field(x, y, w, h)
      {}

  protected:
    bool isOutOfDate() { return drawnValue != getValue(); }

    void redraw() {
      drawnValue = getValue();
      // ensure reset clean drawing environment
      resetText();
      smallText();
      display.setCursor(x, y);
      drawValue(drawnValue);
    }

    virtual void drawValue(const value_t& v) const = 0;
    virtual value_t getValue() const = 0;

  private:
    value_t drawnValue;
  };


  template< typename T >
  class TextField : public ValueField<T> {
  public:
    typedef T value_t;

    TextField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : ValueField<T>(x, y, w, h)
      {}

  protected:
    virtual void drawValue(const value_t& v) const {
      display.print(v);
    }
  };


  class LengthField : public ValueField<EventInterval> {
  public:
    LengthField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : ValueField<EventInterval>(x, y, w, h) { }
  protected:
    void drawValue(const EventInterval& t) const {
      float p = t.inPulses();
      int i10 = int(round(p*10.0f));
      display.printf("%3d.%1ds", i10 / 10, i10 % 10);
    }
    EventInterval getValue() const {
      EventInterval spanT(0);
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


  class ArmedField : public ValueField<bool> {
  public:
    ArmedField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : ValueField<bool>(x, y, w, h) { }
  protected:
    void drawValue(const bool& armed) const {
      if (armed)
        display.print("\xe0");
    }
    bool getValue() const { return currentStatus.armed; }
  };

  class CellCountField : public TextField<int> {
  public:
    CellCountField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : TextField<int>(x, y, w, h) { }
  protected:
    int getValue() const { return Cell::inUse(); }
  };

  class MessageField : public ValueField<std::string> {
  public:
    MessageField(int16_t x, int16_t y, uint16_t w, uint16_t h)
      : ValueField<std::string>(x, y, w, h) { }
  protected:
    void drawValue(const std::string& msg) const {
      display.print(msg.c_str());
    }
    std::string getValue() const { return Message::lastMessage(); }
};

  auto loopField = LoopField(0, 0, 128, 44);
  auto armedField = ArmedField(0, 46, 10, 8);
  auto layerField = LayerField(20, 47, 70, 5);
  auto lengthField = LengthField(92, 46, 36, 8);
  auto msgField = MessageField(0, 56, 90, 8);
  auto cellField = CellCountField(92, 56, 36, 8);

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
    drew |= cellField.render(force);
    drew |= msgField.render(force);

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
