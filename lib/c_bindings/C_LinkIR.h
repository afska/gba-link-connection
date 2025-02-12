#ifndef C_BINDINGS_LINK_IR_H
#define C_BINDINGS_LINK_IR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkIRHandle;

#define C_LINK_IR_DEFAULT_PRIMARY_TIMER_ID 2
#define C_LINK_IR_DEFAULT_SECONDARY_TIMER_ID 3

typedef struct {
  u8 primaryTimerId;
  u8 secondaryTimerId;
} C_LinkIR_Config;

C_LinkIRHandle C_LinkIR_createDefault();
C_LinkIRHandle C_LinkIR_create(u8 primaryTimerId, u8 secondaryTimerId);
void C_LinkIR_destroy(C_LinkIRHandle handle);

bool C_LinkIR_activate(C_LinkIRHandle handle);
void C_LinkIR_deactivate(C_LinkIRHandle handle);

void C_LinkIR_sendNEC(C_LinkIRHandle handle, u8 address, u8 command);
bool C_LinkIR_receiveNEC(C_LinkIRHandle handle,
                         u8* address,
                         u8* command,
                         u32 startTimeout);
bool C_LinkIR_parseNEC(C_LinkIRHandle handle,
                       u16 pulses[],
                       u8* address,
                       u8* command);

void C_LinkIR_send(C_LinkIRHandle handle, u16 pulses[]);
bool C_LinkIR_receive(C_LinkIRHandle handle,
                      u16 pulses[],
                      u32 maxEntries,
                      u32 startTimeout,
                      u32 signalTimeout);

void C_LinkIR_setLight(C_LinkIRHandle handle, bool on);
bool C_LinkIR_isEmittingLight(C_LinkIRHandle handle);
bool C_LinkIR_isDetectingLight(C_LinkIRHandle handle);

C_LinkIR_Config C_LinkIR_getConfig(C_LinkIRHandle handle);
void C_LinkIR_setConfig(C_LinkIRHandle handle, C_LinkIR_Config config);

void C_LinkIR_onSerial(C_LinkIRHandle handle);

extern C_LinkIRHandle cLinkIR;

inline void C_LINK_IR_ISR_SERIAL() {
  C_LinkIR_onSerial(cLinkIR);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_IR_H
