#ifndef C_BINDINGS_LINK_RAW_CABLE_H
#define C_BINDINGS_LINK_RAW_CABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkRawCableHandle;

#define C_LINK_RAW_CABLE_MAX_PLAYERS 4
#define C_LINK_RAW_CABLE_DISCONNECTED 0xffff

typedef enum {
  C_LINK_RAW_CABLE_BAUD_RATE_0,  // 9600 bps
  C_LINK_RAW_CABLE_BAUD_RATE_1,  // 38400 bps
  C_LINK_RAW_CABLE_BAUD_RATE_2,  // 57600 bps
  C_LINK_RAW_CABLE_BAUD_RATE_3   // 115200 bps
} C_LinkRawCable_BaudRate;

typedef enum {
  C_LINK_RAW_CABLE_ASYNC_STATE_IDLE,
  C_LINK_RAW_CABLE_ASYNC_STATE_WAITING,
  C_LINK_RAW_CABLE_ASYNC_STATE_READY
} C_LinkRawCable_AsyncState;

typedef struct {
  u16 data[4];
  int playerId;
} C_LinkRawCable_Response;

C_LinkRawCableHandle C_LinkRawCable_create();
void C_LinkRawCable_destroy(C_LinkRawCableHandle handle);

bool C_LinkRawCable_isActive(C_LinkRawCableHandle handle);
void C_LinkRawCable_activate(C_LinkRawCableHandle handle,
                             C_LinkRawCable_BaudRate baudRate);
void C_LinkRawCable_deactivate(C_LinkRawCableHandle handle);

C_LinkRawCable_Response C_LinkRawCable_transfer(C_LinkRawCableHandle handle,
                                                u16 data);
C_LinkRawCable_Response C_LinkRawCable_transferWithCancel(
    C_LinkRawCableHandle handle,
    u16 data,
    bool (*cancel)());
void C_LinkRawCable_transferAsync(C_LinkRawCableHandle handle, u16 data);

C_LinkRawCable_AsyncState C_LinkRawCable_getAsyncState(
    C_LinkRawCableHandle handle);
C_LinkRawCable_Response C_LinkRawCable_getAsyncData(
    C_LinkRawCableHandle handle);
C_LinkRawCable_BaudRate C_LinkRawCable_getBaudRate(C_LinkRawCableHandle handle);
bool C_LinkRawCable_isMaster(C_LinkRawCableHandle handle);
bool C_LinkRawCable_isReady(C_LinkRawCableHandle handle);

void C_LinkRawCable_onSerial(C_LinkRawCableHandle handle);

extern C_LinkRawCableHandle cLinkRawCable;

inline void C_LINK_RAW_CABLE_ISR_SERIAL() {
  C_LinkRawCable_onSerial(cLinkRawCable);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_RAW_CABLE_H
