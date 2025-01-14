#ifndef C_BINDINGS_LINK_WIRELESS_H
#define C_BINDINGS_LINK_WIRELESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkWirelessHandle;

#define C_LINK_WIRELESS_MAX_PLAYERS 5
#define C_LINK_WIRELESS_MIN_PLAYERS 2
#define C_LINK_WIRELESS_END 0
#define C_LINK_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH 22
#define C_LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 30
#define C_LINK_WIRELESS_BROADCAST_LENGTH 6
#define C_LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH \
  (1 + C_LINK_WIRELESS_BROADCAST_LENGTH)
#define C_LINK_WIRELESS_MAX_SERVERS              \
  (C_LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH / \
   C_LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH)
#define C_LINK_WIRELESS_MAX_GAME_ID 0x7fff
#define C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define C_LINK_WIRELESS_MAX_USER_NAME_LENGTH 8
#define C_LINK_WIRELESS_DEFAULT_TIMEOUT 10
#define C_LINK_WIRELESS_DEFAULT_INTERVAL 50
#define C_LINK_WIRELESS_DEFAULT_SEND_TIMER_ID 3
#define C_LINK_WIRELESS_DEFAULT_ASYNC_ACK_TIMER_ID -1

typedef enum {
  C_LINK_WIRELESS_STATE_NEEDS_RESET,
  C_LINK_WIRELESS_STATE_AUTHENTICATED,
  C_LINK_WIRELESS_STATE_SEARCHING,
  C_LINK_WIRELESS_STATE_SERVING,
  C_LINK_WIRELESS_STATE_CONNECTING,
  C_LINK_WIRELESS_STATE_CONNECTED
} C_LinkWireless_State;

typedef enum {
  C_LINK_WIRELESS_ERROR_NONE,
  C_LINK_WIRELESS_ERROR_WRONG_STATE,
  C_LINK_WIRELESS_ERROR_GAME_NAME_TOO_LONG,
  C_LINK_WIRELESS_ERROR_USER_NAME_TOO_LONG,
  C_LINK_WIRELESS_ERROR_BUFFER_IS_FULL,
  C_LINK_WIRELESS_ERROR_COMMAND_FAILED,
  C_LINK_WIRELESS_ERROR_CONNECTION_FAILED,
  C_LINK_WIRELESS_ERROR_SEND_DATA_FAILED,
  C_LINK_WIRELESS_ERROR_RECEIVE_DATA_FAILED,
  C_LINK_WIRELESS_ERROR_ACKNOWLEDGE_FAILED,
  C_LINK_WIRELESS_ERROR_TIMEOUT,
  C_LINK_WIRELESS_ERROR_REMOTE_TIMEOUT,
  C_LINK_WIRELESS_ERROR_BUSY_TRY_AGAIN
} C_LinkWireless_Error;

typedef struct {
  u16 packetId;
  u16 data;
  u8 playerId;
} C_LinkWireless_Message;

typedef struct {
  u16 id;
  u16 gameId;
  char gameName[C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
  char userName[C_LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1];
  u8 currentPlayerCount;
} C_LinkWireless_Server;

typedef struct {
  bool forwarding;
  bool retransmission;
  u8 maxPlayers;
  u32 timeout;   // can be changed in realtime
  u16 interval;  // can be changed in realtime, but call `resetTimer()`
  u8 sendTimerId;
} C_LinkWireless_Config;

typedef struct {
  u8 signalLevels[C_LINK_WIRELESS_MAX_PLAYERS];
} C_LinkWireless_SignalLevelResponse;

C_LinkWirelessHandle C_LinkWireless_createDefault();
C_LinkWirelessHandle C_LinkWireless_create(bool forwarding,
                                           bool retransmission,
                                           u8 maxPlayers,
                                           u32 timeout,
                                           u16 interval,
                                           u8 sendTimerId);
void C_LinkWireless_destroy(C_LinkWirelessHandle handle);

bool C_LinkWireless_activate(C_LinkWirelessHandle handle);
bool C_LinkWireless_restoreExistingConnection(C_LinkWirelessHandle handle);
bool C_LinkWireless_deactivate(C_LinkWirelessHandle handle);
bool C_LinkWireless_deactivateButKeepOn(C_LinkWirelessHandle handle);

bool C_LinkWireless_serve(C_LinkWirelessHandle handle,
                          const char* gameName,
                          const char* userName,
                          u16 gameId);
bool C_LinkWireless_closeServer(C_LinkWirelessHandle handle);

bool C_LinkWireless_getSignalLevel(
    C_LinkWirelessHandle handle,
    C_LinkWireless_SignalLevelResponse* response);

bool C_LinkWireless_getServers(C_LinkWirelessHandle handle,
                               C_LinkWireless_Server servers[]);
bool C_LinkWireless_getServersAsyncStart(C_LinkWirelessHandle handle);
bool C_LinkWireless_getServersAsyncEnd(C_LinkWirelessHandle handle,
                                       C_LinkWireless_Server servers[]);

bool C_LinkWireless_connect(C_LinkWirelessHandle handle, u16 serverId);
bool C_LinkWireless_keepConnecting(C_LinkWirelessHandle handle);

bool C_LinkWireless_send(C_LinkWirelessHandle handle, u16 data);
bool C_LinkWireless_receive(C_LinkWirelessHandle handle,
                            C_LinkWireless_Message messages[]);

C_LinkWireless_State C_LinkWireless_getState(C_LinkWirelessHandle handle);
bool C_LinkWireless_isConnected(C_LinkWirelessHandle handle);
bool C_LinkWireless_isSessionActive(C_LinkWirelessHandle handle);
bool C_LinkWireless_isServerClosed(C_LinkWirelessHandle handle);
u8 C_LinkWireless_playerCount(C_LinkWirelessHandle handle);
u8 C_LinkWireless_currentPlayerId(C_LinkWirelessHandle handle);
bool C_LinkWireless_didInternalQueueOverflow(C_LinkWirelessHandle handle);
bool C_LinkWireless_didExternalQueueOverflow(C_LinkWirelessHandle handle);

C_LinkWireless_Error C_LinkWireless_getLastError(C_LinkWirelessHandle handle,
                                                 bool clear);

void C_LinkWireless_resetTimer(C_LinkWirelessHandle handle);

C_LinkWireless_Config C_LinkWireless_getConfig(C_LinkWirelessHandle handle);
void C_LinkWireless_setConfig(C_LinkWirelessHandle handle,
                              C_LinkWireless_Config config);

void C_LinkWireless_onVBlank(C_LinkWirelessHandle handle);
void C_LinkWireless_onSerial(C_LinkWirelessHandle handle);
void C_LinkWireless_onTimer(C_LinkWirelessHandle handle);

extern C_LinkWirelessHandle cLinkWireless;

inline void C_LINK_WIRELESS_ISR_VBLANK() {
  C_LinkWireless_onVBlank(cLinkWireless);
}

inline void C_LINK_WIRELESS_ISR_SERIAL() {
  C_LinkWireless_onSerial(cLinkWireless);
}

inline void C_LINK_WIRELESS_ISR_TIMER() {
  C_LinkWireless_onTimer(cLinkWireless);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_WIRELESS_H
