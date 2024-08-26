#ifndef C_BINDINGS_LINK_CABLE_H
#define C_BINDINGS_LINK_CABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkCableHandle;

#define C_LINK_CABLE_MAX_PLAYERS 4
#define C_LINK_CABLE_DEFAULT_TIMEOUT 3
#define C_LINK_CABLE_DEFAULT_INTERVAL 50
#define C_LINK_CABLE_DEFAULT_SEND_TIMER_ID 3
#define C_LINK_CABLE_DISCONNECTED 0xffff
#define C_LINK_CABLE_NO_DATA 0x0

typedef enum {
  C_LINK_CABLE_BAUD_RATE_0,  // 9600 bps
  C_LINK_CABLE_BAUD_RATE_1,  // 38400 bps
  C_LINK_CABLE_BAUD_RATE_2,  // 57600 bps
  C_LINK_CABLE_BAUD_RATE_3   // 115200 bps
} C_LinkCable_BaudRate;

C_LinkCableHandle C_LinkCable_createDefault();
C_LinkCableHandle C_LinkCable_create(C_LinkCable_BaudRate baudRate,
                                     u32 timeout,
                                     u16 interval,
                                     u8 sendTimerId);
void C_LinkCable_destroy(C_LinkCableHandle handle);

bool C_LinkCable_isActive(C_LinkCableHandle handle);
void C_LinkCable_activate(C_LinkCableHandle handle);
void C_LinkCable_deactivate(C_LinkCableHandle handle);

bool C_LinkCable_isConnected(C_LinkCableHandle handle);
u8 C_LinkCable_playerCount(C_LinkCableHandle handle);
u8 C_LinkCable_currentPlayerId(C_LinkCableHandle handle);

void C_LinkCable_sync(C_LinkCableHandle handle);
bool C_LinkCable_waitFor(C_LinkCableHandle handle, u8 playerId);
bool C_LinkCable_waitForWithCancel(C_LinkCableHandle handle,
                                   u8 playerId,
                                   bool (*cancel)());

bool C_LinkCable_canRead(C_LinkCableHandle handle, u8 playerId);
u16 C_LinkCable_read(C_LinkCableHandle handle, u8 playerId);
u16 C_LinkCable_peek(C_LinkCableHandle handle, u8 playerId);

void C_LinkCable_send(C_LinkCableHandle handle, u16 data);

void C_LinkCable_onVBlank(C_LinkCableHandle handle);
void C_LinkCable_onSerial(C_LinkCableHandle handle);
void C_LinkCable_onTimer(C_LinkCableHandle handle);

extern C_LinkCableHandle cLinkCable;

inline void C_LINK_CABLE_ISR_VBLANK() {
  C_LinkCable_onVBlank(cLinkCable);
}

inline void C_LINK_CABLE_ISR_SERIAL() {
  C_LinkCable_onSerial(cLinkCable);
}

inline void C_LINK_CABLE_ISR_TIMER() {
  C_LinkCable_onTimer(cLinkCable);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_CABLE_H
