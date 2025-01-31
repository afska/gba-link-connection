#include "C_LinkRawWireless.h"
#include "../LinkRawWireless.hpp"

extern "C" {
C_LinkRawWireless_CommandResult fromCppResult(
    LinkRawWireless::CommandResult cppResult);
LinkRawWireless::CommandResult toCppResult(
    const C_LinkRawWireless_CommandResult* result);

C_LinkRawWirelessHandle C_LinkRawWireless_create() {
  return new LinkRawWireless();
}

void C_LinkRawWireless_destroy(C_LinkRawWirelessHandle handle) {
  delete static_cast<LinkRawWireless*>(handle);
}

bool C_LinkRawWireless_isActive(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->isActive();
}

bool C_LinkRawWireless_activate(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->activate();
}

bool C_LinkRawWireless_restoreExistingConnection(
    C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->restoreExistingConnection();
}

void C_LinkRawWireless_deactivate(C_LinkRawWirelessHandle handle) {
  static_cast<LinkRawWireless*>(handle)->deactivate();
}

bool C_LinkRawWireless_setup(C_LinkRawWirelessHandle handle,
                             u8 maxPlayers,
                             u8 maxTransmissions,
                             u8 waitTimeout,
                             u32 magic) {
  return static_cast<LinkRawWireless*>(handle)->setup(
      maxPlayers, maxTransmissions, waitTimeout, magic);
}

bool C_LinkRawWireless_getSystemStatus(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_SystemStatusResponse* response) {
  LinkRawWireless::SystemStatusResponse cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->getSystemStatus(cppResponse);
  response->deviceId = cppResponse.deviceId;
  response->currentPlayerId = cppResponse.currentPlayerId;
  response->adapterState =
      static_cast<C_LinkRawWireless_State>(cppResponse.adapterState);
  response->isServerClosed = cppResponse.isServerClosed;

  return success;
}

bool C_LinkRawWireless_broadcast(C_LinkRawWirelessHandle handle,
                                 const char* gameName,
                                 const char* userName,
                                 u16 gameId) {
  return static_cast<LinkRawWireless*>(handle)->broadcast(gameName, userName,
                                                          gameId);
}

bool C_LinkRawWireless_startHost(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->startHost();
}

bool C_LinkRawWireless_startHostNoWait(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->startHost(false);
}

bool C_LinkRawWireless_getSignalLevel(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_SignalLevelResponse* response) {
  LinkRawWireless::SignalLevelResponse cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->getSignalLevel(cppResponse);
  for (u32 i = 0; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++)
    response->signalLevels[i] = cppResponse.signalLevels[i];
  return success;
}

bool C_LinkRawWireless_getSlotStatus(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_SlotStatusResponse* response) {
  LinkRawWireless::SlotStatusResponse cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->getSlotStatus(cppResponse);
  response->nextClientNumber = cppResponse.nextClientNumber;
  response->connectedClientsSize = cppResponse.connectedClientsSize;
  for (u32 i = 0; i < response->connectedClientsSize; i++) {
    response->connectedClients[i].deviceId =
        cppResponse.connectedClients[i].deviceId;
    response->connectedClients[i].clientNumber =
        cppResponse.connectedClients[i].clientNumber;
  }
  return success;
}

bool C_LinkRawWireless_pollConnections(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_PollConnectionsResponse* response) {
  LinkRawWireless::PollConnectionsResponse cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->pollConnections(cppResponse);
  response->connectedClientsSize = cppResponse.connectedClientsSize;
  for (u32 i = 0; i < response->connectedClientsSize; i++) {
    response->connectedClients[i].deviceId =
        cppResponse.connectedClients[i].deviceId;
    response->connectedClients[i].clientNumber =
        cppResponse.connectedClients[i].clientNumber;
  }
  return success;
}

bool C_LinkRawWireless_endHost(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_PollConnectionsResponse* response) {
  LinkRawWireless::PollConnectionsResponse cppResponse;
  bool success = static_cast<LinkRawWireless*>(handle)->endHost(cppResponse);
  response->connectedClientsSize = cppResponse.connectedClientsSize;
  for (u32 i = 0; i < response->connectedClientsSize; i++) {
    response->connectedClients[i].deviceId =
        cppResponse.connectedClients[i].deviceId;
    response->connectedClients[i].clientNumber =
        cppResponse.connectedClients[i].clientNumber;
  }
  return success;
}

bool C_LinkRawWireless_broadcastReadStart(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->broadcastReadStart();
}

bool C_LinkRawWireless_broadcastReadPoll(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_BroadcastReadPollResponse* response) {
  LinkRawWireless::BroadcastReadPollResponse cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->broadcastReadPoll(cppResponse);
  response->serversSize = cppResponse.serversSize;
  for (u32 i = 0; i < response->serversSize; i++) {
    response->servers[i].id = cppResponse.servers[i].id;
    response->servers[i].gameId = cppResponse.servers[i].gameId;
    for (u32 j = 0; j < LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1; j++)
      response->servers[i].gameName[j] = cppResponse.servers[i].gameName[j];
    for (u32 j = 0; j < LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1; j++)
      response->servers[i].userName[j] = cppResponse.servers[i].userName[j];
    response->servers[i].nextClientNumber =
        cppResponse.servers[i].nextClientNumber;
  }
  return success;
}

bool C_LinkRawWireless_broadcastReadEnd(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->broadcastReadEnd();
}

bool C_LinkRawWireless_connect(C_LinkRawWirelessHandle handle, u16 serverId) {
  return static_cast<LinkRawWireless*>(handle)->connect(serverId);
}

bool C_LinkRawWireless_keepConnecting(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_ConnectionStatus* response) {
  LinkRawWireless::ConnectionStatus cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->keepConnecting(cppResponse);
  response->phase =
      static_cast<C_LinkRawWireless_ConnectionPhase>(cppResponse.phase);
  response->assignedClientNumber = cppResponse.assignedClientNumber;
  return success;
}

bool C_LinkRawWireless_finishConnection(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->finishConnection();
}

bool C_LinkRawWireless_sendData(C_LinkRawWirelessHandle handle,
                                const u32* data,
                                u32 dataSize,
                                u32 _bytes) {
  return static_cast<LinkRawWireless*>(handle)->sendData(data, dataSize,
                                                         _bytes);
}

bool C_LinkRawWireless_sendDataAndWait(
    C_LinkRawWirelessHandle handle,
    const u32* data,
    u32 dataSize,
    C_LinkRawWireless_CommandResult* remoteCommand,
    u32 _bytes) {
  LinkRawWireless::CommandResult cppRemoteCommand;
  bool success = static_cast<LinkRawWireless*>(handle)->sendDataAndWait(
      data, dataSize, cppRemoteCommand, _bytes);
  remoteCommand->success = cppRemoteCommand.success;
  remoteCommand->commandId = cppRemoteCommand.commandId;
  remoteCommand->dataSize = cppRemoteCommand.dataSize;
  for (u32 j = 0; j < LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH; j++)
    remoteCommand->data[j] = cppRemoteCommand.data[j];
  return success;
}

bool C_LinkRawWireless_receiveData(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_ReceiveDataResponse* response) {
  LinkRawWireless::ReceiveDataResponse cppResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->receiveData(cppResponse);
  for (u32 i = 0; i < cppResponse.dataSize; i++) {
    response->data[i] = cppResponse.data[i];
  }
  response->dataSize = cppResponse.dataSize;
  for (u32 i = 0; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++) {
    response->sentBytes[i] = cppResponse.sentBytes[i];
  }
  return success;
}

bool C_LinkRawWireless_wait(C_LinkRawWirelessHandle handle,
                            C_LinkRawWireless_CommandResult* remoteCommand) {
  LinkRawWireless::CommandResult cppRemoteCommand;
  bool success = static_cast<LinkRawWireless*>(handle)->wait(cppRemoteCommand);
  remoteCommand->success = cppRemoteCommand.success;
  remoteCommand->commandId = cppRemoteCommand.commandId;
  remoteCommand->dataSize = cppRemoteCommand.dataSize;
  for (u32 j = 0; j < LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH; j++)
    remoteCommand->data[j] = cppRemoteCommand.data[j];
  return success;
}

bool C_LinkRawWireless_disconnectClient(C_LinkRawWirelessHandle handle,
                                        bool client0,
                                        bool client1,
                                        bool client2,
                                        bool client3) {
  return static_cast<LinkRawWireless*>(handle)->disconnectClient(
      client0, client1, client2, client3);
}

bool C_LinkRawWireless_bye(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->bye();
}

u32 C_LinkRawWireless_getSendDataHeaderFor(C_LinkRawWirelessHandle handle,
                                           u32 bytes) {
  return static_cast<LinkRawWireless*>(handle)->getSendDataHeaderFor(bytes);
}

bool C_LinkRawWireless_getReceiveDataResponse(
    C_LinkRawWirelessHandle handle,
    const C_LinkRawWireless_CommandResult* result,
    C_LinkRawWireless_ReceiveDataResponse* response) {
  LinkRawWireless::ReceiveDataResponse cppResponse;
  auto cppResult = toCppResult(result);
  bool success = static_cast<LinkRawWireless*>(handle)->getReceiveDataResponse(
      cppResult, cppResponse);
  for (u32 i = 0; i < cppResponse.dataSize; i++)
    response->data[i] = cppResponse.data[i];
  response->dataSize = cppResponse.dataSize;
  for (u32 i = 0; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++)
    response->sentBytes[i] = cppResponse.sentBytes[i];

  return success;
}

C_LinkRawWireless_CommandResult C_LinkRawWireless_sendCommand(
    C_LinkRawWirelessHandle handle,
    u8 type,
    const u32* params,
    u32 length,
    bool invertsClock) {
  return fromCppResult(static_cast<LinkRawWireless*>(handle)->sendCommand(
      type, params, length, invertsClock));
}

C_LinkRawWireless_CommandResult C_LinkRawWireless_receiveCommandFromAdapter(
    C_LinkRawWirelessHandle handle) {
  return fromCppResult(
      static_cast<LinkRawWireless*>(handle)->receiveCommandFromAdapter());
}

bool C_LinkRawWireless_sendCommandAsync(C_LinkRawWirelessHandle handle,
                                        u8 type,
                                        const u32* params,
                                        u32 length,
                                        bool invertsClock) {
  return static_cast<LinkRawWireless*>(handle)->sendCommandAsync(
      type, params, length, invertsClock);
}

C_LinkRawWireless_AsyncState C_LinkRawWireless_getAsyncState(
    C_LinkRawWirelessHandle handle) {
  return static_cast<C_LinkRawWireless_AsyncState>(
      static_cast<LinkRawWireless*>(handle)->getAsyncState());
}

C_LinkRawWireless_CommandResult C_LinkRawWireless_getAsyncCommandResult(
    C_LinkRawWirelessHandle handle) {
  return fromCppResult(
      static_cast<LinkRawWireless*>(handle)->getAsyncCommandResult());
}

u32 C_LinkRawWireless_getDeviceTransferLength(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->getDeviceTransferLength();
}

C_LinkRawWireless_State C_LinkRawWireless_getState(
    C_LinkRawWirelessHandle handle) {
  return static_cast<C_LinkRawWireless_State>(
      static_cast<LinkRawWireless*>(handle)->getState());
}

bool C_LinkRawWireless_isConnected(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->isConnected();
}

bool C_LinkRawWireless_isSessionActive(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->isSessionActive();
}

bool C_LinkRawWireless_isServerClosed(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->isServerClosed();
}

u8 C_LinkRawWireless_playerCount(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->playerCount();
}

u8 C_LinkRawWireless_currentPlayerId(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->currentPlayerId();
}

void C_LinkRawWireless_onSerial(C_LinkRawWirelessHandle handle) {
  static_cast<LinkRawWireless*>(handle)->_onSerial();
}

C_LinkRawWireless_CommandResult fromCppResult(
    LinkRawWireless::CommandResult cppResult) {
  C_LinkRawWireless_CommandResult result;
  result.success = cppResult.success;
  result.commandId = cppResult.commandId;
  for (u32 i = 0; i < cppResult.dataSize; i++)
    result.data[i] = cppResult.data[i];
  result.dataSize = cppResult.dataSize;
  return result;
}
LinkRawWireless::CommandResult toCppResult(
    const C_LinkRawWireless_CommandResult* result) {
  LinkRawWireless::CommandResult cppResult;
  cppResult.success = cppResult.success;
  cppResult.commandId = result->commandId;
  for (u32 i = 0; i < result->dataSize; i++)
    cppResult.data[i] = result->data[i];
  cppResult.dataSize = result->dataSize;
  return cppResult;
}
}
