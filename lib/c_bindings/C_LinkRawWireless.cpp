#include "C_LinkRawWireless.h"
#include "../LinkRawWireless.hpp"

extern "C" {
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

bool C_LinkRawWireless_deactivate(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->deactivate();
}

bool C_LinkRawWireless_setup(C_LinkRawWirelessHandle handle,
                             u8 maxPlayers,
                             u8 maxTransmissions,
                             u8 waitTimeout,
                             u32 magic) {
  return static_cast<LinkRawWireless*>(handle)->setup(
      maxPlayers, maxTransmissions, waitTimeout, magic);
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

bool C_LinkRawWireless_getSlotStatus(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_SlotStatusResponse* response) {
  LinkRawWireless::SlotStatusResponse nativeResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->getSlotStatus(nativeResponse);
  response->nextClientNumber = nativeResponse.nextClientNumber;
  response->connectedClientsSize = nativeResponse.connectedClientsSize;
  for (u32 i = 0; i < response->connectedClientsSize; i++) {
    response->connectedClients[i].deviceId =
        nativeResponse.connectedClients[i].deviceId;
    response->connectedClients[i].clientNumber =
        nativeResponse.connectedClients[i].clientNumber;
  }
  return success;
}

bool C_LinkRawWireless_acceptConnections(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_AcceptConnectionsResponse* response) {
  LinkRawWireless::AcceptConnectionsResponse nativeResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->acceptConnections(nativeResponse);
  response->connectedClientsSize = nativeResponse.connectedClientsSize;
  for (u32 i = 0; i < response->connectedClientsSize; i++) {
    response->connectedClients[i].deviceId =
        nativeResponse.connectedClients[i].deviceId;
    response->connectedClients[i].clientNumber =
        nativeResponse.connectedClients[i].clientNumber;
  }
  return success;
}

bool C_LinkRawWireless_endHost(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_AcceptConnectionsResponse* response) {
  LinkRawWireless::AcceptConnectionsResponse nativeResponse;
  bool success = static_cast<LinkRawWireless*>(handle)->endHost(nativeResponse);
  response->connectedClientsSize = nativeResponse.connectedClientsSize;
  for (u32 i = 0; i < response->connectedClientsSize; i++) {
    response->connectedClients[i].deviceId =
        nativeResponse.connectedClients[i].deviceId;
    response->connectedClients[i].clientNumber =
        nativeResponse.connectedClients[i].clientNumber;
  }
  return success;
}

bool C_LinkRawWireless_broadcastReadStart(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->broadcastReadStart();
}

bool C_LinkRawWireless_broadcastReadPoll(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_BroadcastReadPollResponse* response) {
  LinkRawWireless::BroadcastReadPollResponse nativeResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->broadcastReadPoll(nativeResponse);
  response->serversSize = nativeResponse.serversSize;
  for (u32 i = 0; i < response->serversSize; i++) {
    response->servers[i].id = nativeResponse.servers[i].id;
    response->servers[i].gameId = nativeResponse.servers[i].gameId;
    std::memcpy(response->servers[i].gameName,
                nativeResponse.servers[i].gameName,
                LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1);
    std::memcpy(response->servers[i].userName,
                nativeResponse.servers[i].userName,
                LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1);
    response->servers[i].nextClientNumber =
        nativeResponse.servers[i].nextClientNumber;
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
  LinkRawWireless::ConnectionStatus nativeResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->keepConnecting(nativeResponse);
  response->phase =
      static_cast<C_LinkRawWireless_ConnectionPhase>(nativeResponse.phase);
  response->assignedClientNumber = nativeResponse.assignedClientNumber;
  return success;
}

bool C_LinkRawWireless_finishConnection(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->finishConnection();
}

bool C_LinkRawWireless_sendData(C_LinkRawWirelessHandle handle,
                                u32* data,
                                u32 dataSize,
                                u32 _bytes) {
  std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> dataArray;
  std::memcpy(dataArray.data(), data, dataSize * sizeof(u32));
  return static_cast<LinkRawWireless*>(handle)->sendData(dataArray, dataSize,
                                                         _bytes);
}

bool C_LinkRawWireless_sendDataAndWait(
    C_LinkRawWirelessHandle handle,
    u32* data,
    u32 dataSize,
    C_LinkRawWireless_RemoteCommand* remoteCommand,
    u32 _bytes) {
  std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> dataArray;
  LinkRawWireless::RemoteCommand nativeRemoteCommand;
  std::memcpy(dataArray.data(), data, dataSize * sizeof(u32));
  bool success = static_cast<LinkRawWireless*>(handle)->sendDataAndWait(
      dataArray, dataSize, nativeRemoteCommand, _bytes);
  remoteCommand->success = nativeRemoteCommand.success;
  remoteCommand->commandId = nativeRemoteCommand.commandId;
  remoteCommand->paramsSize = nativeRemoteCommand.paramsSize;
  std::memcpy(remoteCommand->params, nativeRemoteCommand.params,
              LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH * sizeof(u32));
  return success;
}

bool C_LinkRawWireless_receiveData(
    C_LinkRawWirelessHandle handle,
    C_LinkRawWireless_ReceiveDataResponse* response) {
  LinkRawWireless::ReceiveDataResponse nativeResponse;
  bool success =
      static_cast<LinkRawWireless*>(handle)->receiveData(nativeResponse);
  for (u32 i = 0; i < nativeResponse.dataSize; i++) {
    response->data[i] = nativeResponse.data[i];
  }
  response->dataSize = nativeResponse.dataSize;
  for (u32 i = 0; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++) {
    response->sentBytes[i] = nativeResponse.sentBytes[i];
  }
  return success;
}

bool C_LinkRawWireless_wait(C_LinkRawWirelessHandle handle,
                            C_LinkRawWireless_RemoteCommand* remoteCommand) {
  LinkRawWireless::RemoteCommand nativeRemoteCommand;
  bool success =
      static_cast<LinkRawWireless*>(handle)->wait(nativeRemoteCommand);
  remoteCommand->success = nativeRemoteCommand.success;
  remoteCommand->commandId = nativeRemoteCommand.commandId;
  remoteCommand->paramsSize = nativeRemoteCommand.paramsSize;
  std::memcpy(remoteCommand->params, nativeRemoteCommand.params,
              LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH * sizeof(u32));
  return success;
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

u8 C_LinkRawWireless_playerCount(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->playerCount();
}

u8 C_LinkRawWireless_currentPlayerId(C_LinkRawWirelessHandle handle) {
  return static_cast<LinkRawWireless*>(handle)->currentPlayerId();
}
}
