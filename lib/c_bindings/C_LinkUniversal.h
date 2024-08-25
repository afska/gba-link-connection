#ifndef C_BINDINGS_LINK_UNIVERSAL_H
#define C_BINDINGS_LINK_UNIVERSAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef void* C_LinkUniversalHandle;

#define C_LINK_UNIVERSAL_DISCONNECTED 0xffff
#define C_LINK_UNIVERSAL_NO_DATA 0x0

typedef enum {
  C_STATE_INITIALIZING,
  C_STATE_WAITING,
  C_STATE_CONNECTED
} C_LinkUniversal_State;

typedef enum { C_MODE_LINK_CABLE, C_MODE_LINK_WIRELESS } C_LinkUniversal_Mode;

typedef enum {
  C_PROTOCOL_AUTODETECT,
  C_PROTOCOL_CABLE,
  C_PROTOCOL_WIRELESS_AUTO,
  C_PROTOCOL_WIRELESS_SERVER,
  C_PROTOCOL_WIRELESS_CLIENT
} C_LinkUniversal_Protocol;

typedef struct {
  uint32_t baudRate;
  uint32_t timeout;
  uint16_t interval;
  uint8_t sendTimerId;
} C_LinkUniversal_CableOptions;

typedef struct {
  bool retransmission;
  uint32_t maxPlayers;
  uint32_t timeout;
  uint16_t interval;
  uint8_t sendTimerId;
  int8_t asyncACKTimerId;
} C_LinkUniversal_WirelessOptions;

C_LinkUniversalHandle C_LinkUniversal_create(
    C_LinkUniversal_Protocol protocol,
    const char* gameName,
    C_LinkUniversal_CableOptions cableOptions,
    C_LinkUniversal_WirelessOptions wirelessOptions,
    int randomSeed);
void C_LinkUniversal_destroy(C_LinkUniversalHandle handle);

bool C_LinkUniversal_isActive(C_LinkUniversalHandle handle);
void C_LinkUniversal_activate(C_LinkUniversalHandle handle);
void C_LinkUniversal_deactivate(C_LinkUniversalHandle handle);

bool C_LinkUniversal_isConnected(C_LinkUniversalHandle handle);
uint8_t C_LinkUniversal_playerCount(C_LinkUniversalHandle handle);
uint8_t C_LinkUniversal_currentPlayerId(C_LinkUniversalHandle handle);
void C_LinkUniversal_sync(C_LinkUniversalHandle handle);

bool C_LinkUniversal_waitFor(C_LinkUniversalHandle handle, uint8_t playerId);
bool C_LinkUniversal_waitForWithCancel(C_LinkUniversalHandle handle,
                                       uint8_t playerId,
                                       bool (*cancel)());
bool C_LinkUniversal_canRead(C_LinkUniversalHandle handle, uint8_t playerId);
uint16_t C_LinkUniversal_read(C_LinkUniversalHandle handle, uint8_t playerId);
uint16_t C_LinkUniversal_peek(C_LinkUniversalHandle handle, uint8_t playerId);

bool C_LinkUniversal_send(C_LinkUniversalHandle handle, uint16_t data);
C_LinkUniversal_State C_LinkUniversal_getState(C_LinkUniversalHandle handle);
C_LinkUniversal_Mode C_LinkUniversal_getMode(C_LinkUniversalHandle handle);
C_LinkUniversal_Protocol C_LinkUniversal_getProtocol(
    C_LinkUniversalHandle handle);

void C_LinkUniversal_setProtocol(C_LinkUniversalHandle handle,
                                 C_LinkUniversal_Protocol protocol);

uint32_t C_LinkUniversal_getWaitCount(C_LinkUniversalHandle handle);
uint32_t C_LinkUniversal_getSubWaitCount(C_LinkUniversalHandle handle);

void C_LinkUniversal_onVBlank(C_LinkUniversalHandle handle);
void C_LinkUniversal_onSerial(C_LinkUniversalHandle handle);
void C_LinkUniversal_onTimer(C_LinkUniversalHandle handle);
void C_LinkUniversal_onACKTimer(C_LinkUniversalHandle handle);

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

inline void C_LINK_UNIVERSAL_ISR_ACK_TIMER() {
  C_LinkUniversal_onTimer(cLinkUniversal);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_UNIVERSAL_H
