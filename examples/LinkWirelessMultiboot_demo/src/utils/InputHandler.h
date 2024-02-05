#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <libgba-sprite-engine/gba_engine.h>

class InputHandler {
 public:
  InputHandler() {
    this->isPressed = false;
    this->isWaiting = true;
  }

  inline bool getIsPressed() { return isPressed; }

  inline bool hasBeenPressedNow() { return isNewPressEvent; }
  inline bool hasBeenReleasedNow() { return isNewReleaseEvent; }

  inline bool getHandledFlag() { return handledFlag; }
  inline void setHandledFlag(bool value) { handledFlag = value; }

  inline void setIsPressed(bool isPressed) {
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

#endif  // INPUT_HANDLER_H