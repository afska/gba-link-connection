#include "C_LinkPS2Keyboard.h"
#include "../LinkPS2Keyboard.hpp"

extern "C" {
C_LinkPS2KeyboardHandle C_LinkPS2Keyboard_create(
    C_LinkPS2Keyboard_EventCallback callback) {
  return new LinkPS2Keyboard(callback);
}

void C_LinkPS2Keyboard_destroy(C_LinkPS2KeyboardHandle handle) {
  delete static_cast<LinkPS2Keyboard*>(handle);
}

bool C_LinkPS2Keyboard_isActive(C_LinkPS2KeyboardHandle handle) {
  return static_cast<LinkPS2Keyboard*>(handle)->isActive();
}

void C_LinkPS2Keyboard_activate(C_LinkPS2KeyboardHandle handle) {
  static_cast<LinkPS2Keyboard*>(handle)->activate();
}

void C_LinkPS2Keyboard_deactivate(C_LinkPS2KeyboardHandle handle) {
  static_cast<LinkPS2Keyboard*>(handle)->deactivate();
}

void C_LinkPS2Keyboard_onVBlank(C_LinkPS2KeyboardHandle handle) {
  static_cast<LinkPS2Keyboard*>(handle)->_onVBlank();
}

void C_LinkPS2Keyboard_onSerial(C_LinkPS2KeyboardHandle handle) {
  static_cast<LinkPS2Keyboard*>(handle)->_onSerial();
}
}
