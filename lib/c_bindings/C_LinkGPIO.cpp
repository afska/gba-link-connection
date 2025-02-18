#include "C_LinkGPIO.h"
#include "../LinkGPIO.hpp"

extern "C" {
C_LinkGPIOHandle C_LinkGPIO_create() {
  return new LinkGPIO();
}

void C_LinkGPIO_destroy(C_LinkGPIOHandle handle) {
  delete static_cast<LinkGPIO*>(handle);
}

void C_LinkGPIO_reset(C_LinkGPIOHandle handle) {
  static_cast<LinkGPIO*>(handle)->reset();
}

void C_LinkGPIO_setMode(C_LinkGPIOHandle handle,
                        C_LinkGPIO_Pin pin,
                        C_LinkGPIO_Direction direction) {
  static_cast<LinkGPIO*>(handle)->setMode(
      static_cast<LinkGPIO::Pin>(pin),
      static_cast<LinkGPIO::Direction>(direction));
}

C_LinkGPIO_Direction C_LinkGPIO_getMode(C_LinkGPIOHandle handle,
                                        C_LinkGPIO_Pin pin) {
  return static_cast<C_LinkGPIO_Direction>(
      static_cast<LinkGPIO*>(handle)->getMode(static_cast<LinkGPIO::Pin>(pin)));
}

bool C_LinkGPIO_readPin(C_LinkGPIOHandle handle, C_LinkGPIO_Pin pin) {
  return static_cast<LinkGPIO*>(handle)->readPin(
      static_cast<LinkGPIO::Pin>(pin));
}

void C_LinkGPIO_writePin(C_LinkGPIOHandle handle,
                         C_LinkGPIO_Pin pin,
                         bool isHigh) {
  static_cast<LinkGPIO*>(handle)->writePin(static_cast<LinkGPIO::Pin>(pin),
                                           isHigh);
}

void C_LinkGPIO_setSIInterrupts(C_LinkGPIOHandle handle, bool isEnabled) {
  static_cast<LinkGPIO*>(handle)->setSIInterrupts(isEnabled);
}

bool C_LinkGPIO_getSIInterrupts(C_LinkGPIOHandle handle) {
  return static_cast<LinkGPIO*>(handle)->getSIInterrupts();
}
}
