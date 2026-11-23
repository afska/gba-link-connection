#ifndef C_BINDINGS_LINK_UNIVERSAL_H
#define C_BINDINGS_LINK_UNIVERSAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "C_LinkCable.h"
#include "C_LinkWireless.h"
#include "_c_link_common.h"

typedef void* C_LinkUniversalHandle;

#define C_LINK_UNIVERSAL_DISCONNECTED 0xFFFF
#define C_LINK_UNIVERSAL_NO_DATA 0x0

typedef enum {
  C_LINK_UNIVERSAL_STATE_INITIALIZING,
  C_LINK_UNIVERSAL_STATE_WAITING,
  C_LINK_UNIVERSAL_STATE_CONNECTED
} C_LinkUniversal_State;

typedef enum {
  C_LINK_UNIVERSAL_MODE_LINK_CABLE,
  C_LINK_UNIVERSAL_MODE_LINK_WIRELESS
} C_LinkUniversal_Mode;

typedef enum {
  C_LINK_UNIVERSAL_PROTOCOL_AUTODETECT,
  C_LINK_UNIVERSAL_PROTOCOL_CABLE,
  C_LINK_UNIVERSAL_PROTOCOL_WIRELESS_AUTO,
  C_LINK_UNIVERSAL_PROTOCOL_WIRELESS_SERVER,
  C_LINK_UNIVERSAL_PROTOCOL_WIRELESS_CLIENT,
  C_LINK_UNIVERSAL_PROTOCOL_WIRELESS_RESTORE_EXISTING
} C_LinkUniversal_Protocol;

typedef struct {
  _u32 baudRate;
  _u32 timeout;
  _u16 interval;
  _u8 sendTimerId;
} C_LinkUniversal_CableOptions;

typedef struct {
  bool forwarding;
  bool retransmission;
  _u32 maxPlayers;
  _u32 timeout;
  _u16 interval;
  _u8 sendTimerId;
} C_LinkUniversal_WirelessOptions;

C_LinkUniversalHandle C_LinkUniversal_createDefault();
C_LinkUniversalHandle C_LinkUniversal_create(
    C_LinkUniversal_Protocol protocol,
    const char* gameName,
    C_LinkUniversal_CableOptions cableOptions,
    C_LinkUniversal_WirelessOptions wirelessOptions);
void C_LinkUniversal_destroy(C_LinkUniversalHandle handle);

bool C_LinkUniversal_isActive(C_LinkUniversalHandle handle);
void C_LinkUniversal_activate(C_LinkUniversalHandle handle);
bool C_LinkUniversal_deactivate(C_LinkUniversalHandle handle);
bool C_LinkUniversal_deactivateButKeepWirelessOn(C_LinkUniversalHandle handle);

bool C_LinkUniversal_isConnected(C_LinkUniversalHandle handle);
_u8 C_LinkUniversal_playerCount(C_LinkUniversalHandle handle);
_u8 C_LinkUniversal_currentPlayerId(C_LinkUniversalHandle handle);

void C_LinkUniversal_sync(C_LinkUniversalHandle handle);
bool C_LinkUniversal_waitFor(C_LinkUniversalHandle handle, _u8 playerId);
bool C_LinkUniversal_waitForWithCancel(C_LinkUniversalHandle handle,
                                       _u8 playerId,
                                       bool (*cancel)());

bool C_LinkUniversal_canRead(C_LinkUniversalHandle handle, _u8 playerId);
_u16 C_LinkUniversal_read(C_LinkUniversalHandle handle, _u8 playerId);
_u16 C_LinkUniversal_peek(C_LinkUniversalHandle handle, _u8 playerId);

bool C_LinkUniversal_canSend(C_LinkUniversalHandle handle);
bool C_LinkUniversal_send(C_LinkUniversalHandle handle, _u16 data);

bool C_LinkUniversal_didQueueOverflow(C_LinkUniversalHandle handle, bool clear);

void C_LinkUniversal_resetTimeout(C_LinkUniversalHandle handle);
void C_LinkUniversal_resetTimer(C_LinkUniversalHandle handle);

C_LinkUniversal_State C_LinkUniversal_getState(C_LinkUniversalHandle handle);
C_LinkUniversal_Mode C_LinkUniversal_getMode(C_LinkUniversalHandle handle);

C_LinkUniversal_Protocol C_LinkUniversal_getProtocol(
    C_LinkUniversalHandle handle);
void C_LinkUniversal_setProtocol(C_LinkUniversalHandle handle,
                                 C_LinkUniversal_Protocol protocol);

bool C_LinkUniversal_isConnectedNow(C_LinkUniversalHandle handle);
C_LinkWireless_State C_LinkUniversal_getWirelessState(
    C_LinkUniversalHandle handle);

C_LinkCableHandle C_LinkUniversal_getLinkCable(C_LinkUniversalHandle handle);
C_LinkWirelessHandle C_LinkUniversal_getLinkWireless(
    C_LinkUniversalHandle handle);

_u32 C_LinkUniversal_getWaitCount(C_LinkUniversalHandle handle);
_u32 C_LinkUniversal_getSubWaitCount(C_LinkUniversalHandle handle);

void C_LinkUniversal_onVBlank(C_LinkUniversalHandle handle);
void C_LinkUniversal_onSerial(C_LinkUniversalHandle handle);
void C_LinkUniversal_onTimer(C_LinkUniversalHandle handle);

extern C_LinkUniversalHandle cLinkUniversal;

inline void C_LINK_UNIVERSAL_ISR_VBLANK() {
  C_LinkUniversal_onVBlank(cLinkUniversal);
}

inline void C_LINK_UNIVERSAL_ISR_SERIAL() {
  C_LinkUniversal_onSerial(cLinkUniversal);
}

inline void C_LINK_UNIVERSAL_ISR_TIMER() {
  C_LinkUniversal_onTimer(cLinkUniversal);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_UNIVERSAL_H
