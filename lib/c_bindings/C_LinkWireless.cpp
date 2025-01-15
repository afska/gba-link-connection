#include "C_LinkWireless.h"
#include "../LinkWireless.hpp"

extern "C" {

C_LinkWirelessHandle C_LinkWireless_createDefault() {
  return new LinkWireless();
}

C_LinkWirelessHandle C_LinkWireless_create(bool forwarding,
                                           bool retransmission,
                                           u8 maxPlayers,
                                           u32 timeout,
                                           u16 interval,
                                           u8 sendTimerId) {
  return new LinkWireless(forwarding, retransmission, maxPlayers, timeout,
                          interval, sendTimerId);
}

void C_LinkWireless_destroy(C_LinkWirelessHandle handle) {
  delete static_cast<LinkWireless*>(handle);
}

bool C_LinkWireless_activate(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->activate();
}

bool C_LinkWireless_restoreExistingConnection(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->restoreExistingConnection();
}

bool C_LinkWireless_deactivate(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->deactivate();
}

bool C_LinkWireless_deactivateButKeepOn(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->deactivate(false);
}

bool C_LinkWireless_serve(C_LinkWirelessHandle handle,
                          const char* gameName,
                          const char* userName,
                          u16 gameId) {
  return static_cast<LinkWireless*>(handle)->serve(gameName, userName, gameId);
}

bool C_LinkWireless_closeServer(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->closeServer();
}

bool C_LinkWireless_getSignalLevel(
    C_LinkWirelessHandle handle,
    C_LinkWireless_SignalLevelResponse* response) {
  LinkWireless::SignalLevelResponse cppResponse;
  bool success =
      static_cast<LinkWireless*>(handle)->getSignalLevel(cppResponse);
  for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++)
    response->signalLevels[i] = cppResponse.signalLevels[i];
  return success;
}

bool C_LinkWireless_getServers(C_LinkWirelessHandle handle,
                               C_LinkWireless_Server servers[]) {
  LinkWireless::Server cppServers[C_LINK_WIRELESS_MAX_SERVERS];
  bool result = static_cast<LinkWireless*>(handle)->getServers(cppServers);

  for (u32 i = 0; i < C_LINK_WIRELESS_MAX_SERVERS; i++) {
    servers[i].id = cppServers[i].id;
    servers[i].gameId = cppServers[i].gameId;
    for (u32 j = 0; j < C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1; j++)
      servers[i].gameName[j] = cppServers[i].gameName[j];
    for (u32 j = 0; j < C_LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1; j++)
      servers[i].userName[j] = cppServers[i].userName[j];
    servers[i].currentPlayerCount = cppServers[i].currentPlayerCount;
  }

  return result;
}

bool C_LinkWireless_getServersAsyncStart(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->getServersAsyncStart();
}

bool C_LinkWireless_getServersAsyncEnd(C_LinkWirelessHandle handle,
                                       C_LinkWireless_Server servers[]) {
  LinkWireless::Server cppServers[C_LINK_WIRELESS_MAX_SERVERS];
  bool result =
      static_cast<LinkWireless*>(handle)->getServersAsyncEnd(cppServers);

  for (u32 i = 0; i < C_LINK_WIRELESS_MAX_SERVERS; i++) {
    servers[i].id = cppServers[i].id;
    servers[i].gameId = cppServers[i].gameId;
    for (u32 j = 0; j < C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1; j++)
      servers[i].gameName[j] = cppServers[i].gameName[j];
    for (u32 j = 0; j < C_LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1; j++)
      servers[i].userName[j] = cppServers[i].userName[j];
    servers[i].currentPlayerCount = cppServers[i].currentPlayerCount;
  }

  return result;
}

bool C_LinkWireless_connect(C_LinkWirelessHandle handle, u16 serverId) {
  return static_cast<LinkWireless*>(handle)->connect(serverId);
}

bool C_LinkWireless_keepConnecting(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->keepConnecting();
}

bool C_LinkWireless_send(C_LinkWirelessHandle handle, u16 data) {
  return static_cast<LinkWireless*>(handle)->send(data);
}

bool C_LinkWireless_receive(C_LinkWirelessHandle handle,
                            C_LinkWireless_Message messages[]) {
  LinkWireless::Message cppMessages[C_LINK_WIRELESS_MAX_PLAYERS];
  bool result = static_cast<LinkWireless*>(handle)->receive(cppMessages);

  for (int i = 0; i < C_LINK_WIRELESS_MAX_PLAYERS; i++) {
    messages[i].packetId = cppMessages[i].packetId;
    messages[i].data = cppMessages[i].data;
    messages[i].playerId = cppMessages[i].playerId;
  }

  return result;
}

C_LinkWireless_State C_LinkWireless_getState(C_LinkWirelessHandle handle) {
  return static_cast<C_LinkWireless_State>(
      static_cast<LinkWireless*>(handle)->getState());
}

bool C_LinkWireless_isConnected(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->isConnected();
}

bool C_LinkWireless_isSessionActive(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->isSessionActive();
}

bool C_LinkWireless_isServerClosed(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->isServerClosed();
}

u8 C_LinkWireless_playerCount(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->playerCount();
}

u8 C_LinkWireless_currentPlayerId(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->currentPlayerId();
}

bool C_LinkWireless_didQueueOverflow(C_LinkWirelessHandle handle, bool clear) {
  return static_cast<LinkWireless*>(handle)->didQueueOverflow(clear);
}

C_LinkWireless_Error C_LinkWireless_getLastError(C_LinkWirelessHandle handle,
                                                 bool clear) {
  return static_cast<C_LinkWireless_Error>(
      static_cast<LinkWireless*>(handle)->getLastError(clear));
}

void C_LinkWireless_resetTimer(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->resetTimer();
}

C_LinkWireless_Config C_LinkWireless_getConfig(C_LinkWirelessHandle handle) {
  C_LinkWireless_Config config;
  config.forwarding = static_cast<LinkWireless*>(handle)->config.forwarding;
  config.retransmission =
      static_cast<LinkWireless*>(handle)->config.retransmission;
  config.maxPlayers = static_cast<LinkWireless*>(handle)->config.maxPlayers;
  config.timeout = static_cast<LinkWireless*>(handle)->config.timeout;
  config.interval = static_cast<LinkWireless*>(handle)->config.interval;
  config.sendTimerId = static_cast<LinkWireless*>(handle)->config.sendTimerId;
  return config;
}

void C_LinkWireless_setConfig(C_LinkWirelessHandle handle,
                              C_LinkWireless_Config config) {
  static_cast<LinkWireless*>(handle)->config.forwarding = config.forwarding;
  static_cast<LinkWireless*>(handle)->config.retransmission =
      config.retransmission;
  static_cast<LinkWireless*>(handle)->config.maxPlayers = config.maxPlayers;
  static_cast<LinkWireless*>(handle)->config.timeout = config.timeout;
  static_cast<LinkWireless*>(handle)->config.interval = config.interval;
  static_cast<LinkWireless*>(handle)->config.sendTimerId = config.sendTimerId;
}

void C_LinkWireless_onVBlank(C_LinkWirelessHandle handle) {
  static_cast<LinkWireless*>(handle)->_onVBlank();
}

void C_LinkWireless_onSerial(C_LinkWirelessHandle handle) {
  static_cast<LinkWireless*>(handle)->_onSerial();
}

void C_LinkWireless_onTimer(C_LinkWirelessHandle handle) {
  static_cast<LinkWireless*>(handle)->_onTimer();
}
}
