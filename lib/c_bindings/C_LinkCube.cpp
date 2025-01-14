#include "C_LinkCube.h"
#include "../LinkCube.hpp"

extern "C" {

C_LinkCubeHandle C_LinkCube_create() {
  return new LinkCube();
}

void C_LinkCube_destroy(C_LinkCubeHandle handle) {
  delete static_cast<LinkCube*>(handle);
}

bool C_LinkCube_isActive(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->isActive();
}

void C_LinkCube_activate(C_LinkCubeHandle handle) {
  static_cast<LinkCube*>(handle)->activate();
}

void C_LinkCube_deactivate(C_LinkCubeHandle handle) {
  static_cast<LinkCube*>(handle)->deactivate();
}

bool C_LinkCube_wait(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->wait();
}

bool C_LinkCube_waitWithCancel(C_LinkCubeHandle handle, bool (*cancel)()) {
  return static_cast<LinkCube*>(handle)->wait(cancel);
}

bool C_LinkCube_canRead(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->canRead();
}

u32 C_LinkCube_read(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->read();
}

u32 C_LinkCube_peek(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->peek();
}

void C_LinkCube_send(C_LinkCubeHandle handle, u32 data) {
  static_cast<LinkCube*>(handle)->send(data);
}

u32 C_LinkCube_pendingCount(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->pendingCount();
}

bool C_LinkCube_didInternalQueueOverflow(C_LinkCubeHandle handle) {
  return static_cast<LinkCube*>(handle)->didInternalQueueOverflow();
}

bool C_LinkCube_didReset(C_LinkCubeHandle handle, bool clear) {
  return static_cast<LinkCube*>(handle)->didReset(clear);
}

void C_LinkCube_onSerial(C_LinkCubeHandle handle) {
  static_cast<LinkCube*>(handle)->_onSerial();
}
}
