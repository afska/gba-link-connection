#include "C_LinkPS2Mouse.h"
#include "../LinkPS2Mouse.hpp"

extern "C" {
C_LinkPS2MouseHandle C_LinkPS2Mouse_create(u8 waitTimerId) {
  return new LinkPS2Mouse(waitTimerId);
}

void C_LinkPS2Mouse_destroy(C_LinkPS2MouseHandle handle) {
  delete static_cast<LinkPS2Mouse*>(handle);
}

bool C_LinkPS2Mouse_isActive(C_LinkPS2MouseHandle handle) {
  return static_cast<LinkPS2Mouse*>(handle)->isActive();
}

void C_LinkPS2Mouse_activate(C_LinkPS2MouseHandle handle) {
  static_cast<LinkPS2Mouse*>(handle)->activate();
}

void C_LinkPS2Mouse_deactivate(C_LinkPS2MouseHandle handle) {
  static_cast<LinkPS2Mouse*>(handle)->deactivate();
}

void C_LinkPS2Mouse_report(C_LinkPS2MouseHandle handle, int data[3]) {
  int d[3];
  static_cast<LinkPS2Mouse*>(handle)->report(d);
  for (u32 i = 0; i < 3; i++)
    data[i] = d[i];
}
}
