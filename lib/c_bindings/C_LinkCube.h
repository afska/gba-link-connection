#ifndef C_BINDINGS_LINK_CUBE_H
#define C_BINDINGS_LINK_CUBE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkCubeHandle;

C_LinkCubeHandle C_LinkCube_create();
void C_LinkCube_destroy(C_LinkCubeHandle handle);

bool C_LinkCube_isActive(C_LinkCubeHandle handle);
void C_LinkCube_activate(C_LinkCubeHandle handle);
void C_LinkCube_deactivate(C_LinkCubeHandle handle);

bool C_LinkCube_wait(C_LinkCubeHandle handle);
bool C_LinkCube_waitWithCancel(C_LinkCubeHandle handle, bool (*cancel)());

bool C_LinkCube_canRead(C_LinkCubeHandle handle);
u32 C_LinkCube_read(C_LinkCubeHandle handle);
u32 C_LinkCube_peek(C_LinkCubeHandle handle);

void C_LinkCube_send(C_LinkCubeHandle handle, u32 data);
u32 C_LinkCube_pendingCount(C_LinkCubeHandle handle);
bool C_LinkCube_didQueueOverflow(C_LinkCubeHandle handle);

bool C_LinkCube_didReset(C_LinkCubeHandle handle, bool clear);

void C_LinkCube_onSerial(C_LinkCubeHandle handle);

extern C_LinkCubeHandle cLinkCube;

inline void C_LINK_CUBE_ISR_SERIAL() {
  C_LinkCube_onSerial(cLinkCube);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_CUBE_H
