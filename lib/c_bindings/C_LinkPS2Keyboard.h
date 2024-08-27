#ifndef C_BINDINGS_LINK_PS2_KEYBOARD_H
#define C_BINDINGS_LINK_PS2_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void (*C_LinkPS2Keyboard_EventCallback)(u8 event);

typedef void* C_LinkPS2KeyboardHandle;

C_LinkPS2KeyboardHandle C_LinkPS2Keyboard_create(
    C_LinkPS2Keyboard_EventCallback callback);
void C_LinkPS2Keyboard_destroy(C_LinkPS2KeyboardHandle handle);

bool C_LinkPS2Keyboard_isActive(C_LinkPS2KeyboardHandle handle);
void C_LinkPS2Keyboard_activate(C_LinkPS2KeyboardHandle handle);
void C_LinkPS2Keyboard_deactivate(C_LinkPS2KeyboardHandle handle);

void C_LinkPS2Keyboard_onVBlank(C_LinkPS2KeyboardHandle handle);
void C_LinkPS2Keyboard_onSerial(C_LinkPS2KeyboardHandle handle);

extern C_LinkPS2KeyboardHandle cLinkPS2Keyboard;

inline void C_LINK_PS2_KEYBOARD_ISR_VBLANK() {
  C_LinkPS2Keyboard_onVBlank(cLinkPS2Keyboard);
}

inline void C_LINK_PS2_KEYBOARD_ISR_SERIAL() {
  C_LinkPS2Keyboard_onSerial(cLinkPS2Keyboard);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_PS2_KEYBOARD_H
