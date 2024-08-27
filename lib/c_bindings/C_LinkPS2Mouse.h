#ifndef C_BINDINGS_LINK_PS2_MOUSE_H
#define C_BINDINGS_LINK_PS2_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkPS2MouseHandle;

#define C_LINK_PS2_MOUSE_LEFT_CLICK 0b001
#define C_LINK_PS2_MOUSE_RIGHT_CLICK 0b010
#define C_LINK_PS2_MOUSE_MIDDLE_CLICK 0b100

C_LinkPS2MouseHandle C_LinkPS2Mouse_create(u8 waitTimerId);
void C_LinkPS2Mouse_destroy(C_LinkPS2MouseHandle handle);

bool C_LinkPS2Mouse_isActive(C_LinkPS2MouseHandle handle);
void C_LinkPS2Mouse_activate(C_LinkPS2MouseHandle handle);
void C_LinkPS2Mouse_deactivate(C_LinkPS2MouseHandle handle);

void C_LinkPS2Mouse_report(C_LinkPS2MouseHandle handle, int data[3]);

extern C_LinkPS2MouseHandle cLinkPS2Mouse;

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_PS2_MOUSE_H
