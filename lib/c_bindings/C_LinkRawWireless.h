#ifndef C_BINDINGS_LINK_RAW_WIRELESS_H
#define C_BINDINGS_LINK_RAW_WIRELESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkRawWirelessHandle;

#define C_LINK_RAW_WIRELESS_MAX_PLAYERS 5
#define C_LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 30
#define C_LINK_RAW_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#define C_LINK_RAW_WIRELESS_MAX_GAME_ID 0x7fff
#define C_LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define C_LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH 8
#define C_LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH 23
#define C_LINK_RAW_WIRELESS_SETUP_MAGIC 0x003c0000

#define C_LINK_RAW_WIRELESS_MAX_SERVERS 4

typedef enum {
  C_LINK_RAW_WIRELESS_STATE_NEEDS_RESET,
  C_LINK_RAW_WIRELESS_STATE_AUTHENTICATED,
  C_LINK_RAW_WIRELESS_STATE_SEARCHING,
  C_LINK_RAW_WIRELESS_STATE_SERVING,
  C_LINK_RAW_WIRELESS_STATE_CONNECTING,
  C_LINK_RAW_WIRELESS_STATE_CONNECTED
} C_LinkRawWireless_State;

typedef enum {
  C_LINK_RAW_WIRELESS_ASYNC_STATE_IDLE,
  C_LINK_RAW_WIRELESS_ASYNC_STATE_WORKING,
  C_LINK_RAW_WIRELESS_ASYNC_STATE_READY
} C_LinkRawWireless_AsyncState;

typedef struct {
  bool success;
  u8 commandId;
  u32 data[C_LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH];
  u32 dataSize;
} C_LinkRawWireless_CommandResult;

typedef struct {
  u16 id;
  u16 gameId;
  char gameName[C_LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
  char userName[C_LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1];
  u8 nextClientNumber;
} C_LinkRawWireless_Server;

typedef struct {
  u16 deviceId;
  u8 clientNumber;
} C_LinkRawWireless_ConnectedClient;

typedef struct {
  u16 deviceId;
  u8 currentPlayerId;
  C_LinkRawWireless_State adapterState;
  bool isServerClosed;
} C_LinkRawWireless_SystemStatusResponse;

typedef struct {
  u8 nextClientNumber;
  C_LinkRawWireless_ConnectedClient
      connectedClients[C_LINK_RAW_WIRELESS_MAX_PLAYERS];
  u32 connectedClientsSize;
} C_LinkRawWireless_SlotStatusResponse;

typedef struct {
  C_LinkRawWireless_ConnectedClient
      connectedClients[C_LINK_RAW_WIRELESS_MAX_PLAYERS];
  u32 connectedClientsSize;
} C_LinkRawWireless_AcceptConnectionsResponse;

typedef struct {
  C_LinkRawWireless_Server servers[C_LINK_RAW_WIRELESS_MAX_SERVERS];
  u32 serversSize;
} C_LinkRawWireless_BroadcastReadPollResponse;

typedef enum {
  C_LINK_RAW_WIRELESS_CONNECTION_PHASE_STILL_CONNECTING,
  C_LINK_RAW_WIRELESS_CONNECTION_PHASE_ERROR,
  C_LINK_RAW_WIRELESS_CONNECTION_PHASE_SUCCESS
} C_LinkRawWireless_ConnectionPhase;

typedef struct {
  C_LinkRawWireless_ConnectionPhase phase;
  u8 assignedClientNumber;
} C_LinkRawWireless_ConnectionStatus;

typedef struct {
  u32 sentBytes[C_LINK_RAW_WIRELESS_MAX_PLAYERS];
  u32 data[C_LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
  u32 dataSize;
} C_LinkRawWireless_ReceiveDataResponse;

C_LinkRawWirelessHandle C_LinkRawWireless_create();
void C_LinkRawWireless_destroy(C_LinkRawWirelessHandle handle);

bool C_LinkRawWireless_isActive(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_activate(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_restoreExistingConnection(
    C_LinkRawWirelessHandle handle);
void C_LinkRawWireless_deactivate(C_LinkRawWirelessHandle handle);

bool C_LinkRawWireless_setup(C_LinkRawWirelessHandle handle,
                             u8 maxPlayers,
                             u8 maxTransmissions,
                             u8 waitTimeout,
                             u32 magic);
bool C_LinkRawWireless_broadcast(C_LinkRawWirelessHandle handle,
                                 const char* gameName,
                                 const char* userName,
                                 u16 gameId);
bool C_LinkRawWireless_startHost(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_getSystemStatus(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_SystemStatusResponse* response);
bool C_LinkRawWireless_getSlotStatus(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_SlotStatusResponse* response);
bool C_LinkRawWireless_acceptConnections(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_AcceptConnectionsResponse* response);
bool C_LinkRawWireless_endHost(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_AcceptConnectionsResponse* response);
bool C_LinkRawWireless_broadcastReadStart(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_broadcastReadPoll(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_BroadcastReadPollResponse* response);
bool C_LinkRawWireless_broadcastReadEnd(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_connect(C_LinkRawWirelessHandle handle, u16 serverId);
bool C_LinkRawWireless_keepConnecting(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_ConnectionStatus* response);
bool C_LinkRawWireless_finishConnection(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_sendData(C_LinkRawWirelessHandle handle,
                                const u32* data,
                                u32 dataSize,
                                u32 _bytes);
bool C_LinkRawWireless_sendDataAndWait(
    C_LinkRawWirelessHandle handle,
    const u32* data,
    u32 dataSize,
    C_LinkRawWireless_CommandResult* remoteCommand,
    u32 _bytes);
bool C_LinkRawWireless_receiveData(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_ReceiveDataResponse* response);
bool C_LinkRawWireless_wait(C_LinkRawWirelessHandle handle,
                            C_LinkRawWireless_CommandResult* remoteCommand);

bool C_LinkRawWireless_bye(C_LinkRawWirelessHandle handle);

u32 C_LinkRawWireless_getSendDataHeaderFor(C_LinkRawWirelessHandle handle,
                                           u32 bytes);
bool C_LinkRawWireless_getReceiveDataResponse(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_CommandResult result,
    C_LinkRawWireless_ReceiveDataResponse* response);

C_LinkRawWireless_CommandResult C_LinkRawWireless_sendCommand(
    C_LinkRawWirelessHandle handle,
    u8 type,
    const u32* params,
    u32 length,
    bool invertsClock);
C_LinkRawWireless_CommandResult C_LinkRawWireless_receiveCommandFromAdapter(
    C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_sendCommandAsync(C_LinkRawWirelessHandle handle,
                                        u8 type,
                                        const u32* params,
                                        u32 length,
                                        bool invertsClock);

C_LinkRawWireless_AsyncState C_LinkRawWireless_getAsyncState(
    C_LinkRawWirelessHandle handle);
C_LinkRawWireless_CommandResult C_LinkRawWireless_getAsyncCommandResult(
    C_LinkRawWirelessHandle handle);

u32 C_LinkRawWireless_getDeviceTransferLength(C_LinkRawWirelessHandle handle);
C_LinkRawWireless_State C_LinkRawWireless_getState(
    C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_isConnected(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_isSessionActive(C_LinkRawWirelessHandle handle);
bool C_LinkRawWireless_isServerClosed(C_LinkRawWirelessHandle handle);
u8 C_LinkRawWireless_playerCount(C_LinkRawWirelessHandle handle);
u8 C_LinkRawWireless_currentPlayerId(C_LinkRawWirelessHandle handle);

void C_LinkRawWireless_onSerial(C_LinkRawWirelessHandle handle);

extern C_LinkRawWirelessHandle cLinkRawWireless;

inline void C_LINK_LINK_RAW_WIRELESS_ISR_SERIAL() {
  C_LinkRawWireless_onSerial(cLinkRawWireless);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_RAW_WIRELESS_H
