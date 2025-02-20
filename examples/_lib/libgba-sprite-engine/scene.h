#ifndef LINK_EXAMPLES_SCENE_H
#define LINK_EXAMPLES_SCENE_H

#include <libgba-sprite-engine/background/text_stream.h>
#include <tonc_memdef.h>
#include <tonc_memmap.h>
#include <string>

const u32 TEXT_MIDDLE_COL = 12;

extern int DEBULOG_LINE;
void DEBULOG(std::string string);

class InputHandler {
 public:
  InputHandler() {
    this->isPressed = false;
    this->isWaiting = true;
  }

  bool getIsPressed() { return isPressed; }

  bool hasBeenPressedNow() { return isNewPressEvent; }
  bool hasBeenReleasedNow() { return isNewReleaseEvent; }

  bool getHandledFlag() { return handledFlag; }
  void setHandledFlag(bool value) { handledFlag = value; }

  void setIsPressed(bool isPressed) {
    bool isNewPressEvent = !this->isWaiting && !this->isPressed && isPressed;
    bool isNewReleaseEvent = !this->isWaiting && this->isPressed && !isPressed;
    this->isPressed = isPressed;
    this->isWaiting = this->isWaiting && isPressed;

    this->isNewPressEvent = isNewPressEvent;
    this->isNewReleaseEvent = isNewReleaseEvent;
  }

 protected:
  bool isPressed = false;
  bool isNewPressEvent = false;
  bool isNewReleaseEvent = false;
  bool handledFlag = false;
  bool isWaiting = false;
};

inline void BACKGROUND_enable(bool bg0, bool bg1, bool bg2, bool bg3) {
  REG_DISPCNT = bg0 ? REG_DISPCNT | DCNT_BG0 : REG_DISPCNT & ~DCNT_BG0;
  REG_DISPCNT = bg1 ? REG_DISPCNT | DCNT_BG1 : REG_DISPCNT & ~DCNT_BG1;
  REG_DISPCNT = bg2 ? REG_DISPCNT | DCNT_BG2 : REG_DISPCNT & ~DCNT_BG2;
  REG_DISPCNT = bg3 ? REG_DISPCNT | DCNT_BG3 : REG_DISPCNT & ~DCNT_BG3;
}

inline void SPRITE_disable() {
  REG_DISPCNT = REG_DISPCNT & ~DCNT_OBJ;
}

inline void SCENE_init() {
  TextStream::instance().clear();
  TextStream::instance().scroll(0, 0);
  TextStream::instance().setMosaic(false);

  BACKGROUND_enable(false, false, false, false);
  SPRITE_disable();
}

inline void SCENE_write(std::string text, u32 row) {
  TextStream::instance().setText(text, row,
                                 TEXT_MIDDLE_COL - text.length() / 2);
}

#endif  // LINK_EXAMPLES_SCENE_H
