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
                                           u8 sendTimerId,
                                           s8 asyncACKTimerId) {
  return new LinkWireless(forwarding, retransmission, maxPlayers, timeout,
                          interval, sendTimerId, asyncACKTimerId);
}

void C_LinkWireless_destroy(C_LinkWirelessHandle handle) {
  delete static_cast<LinkWireless*>(handle);
}

bool C_LinkWireless_activate(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->activate();
}

bool C_LinkWireless_deactivate(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->deactivate();
}

bool C_LinkWireless_serve(C_LinkWirelessHandle handle,
                          const char* gameName,
                          const char* userName,
                          u16 gameId) {
  return static_cast<LinkWireless*>(handle)->serve(gameName, userName, gameId);
}

bool C_LinkWireless_getServers(C_LinkWirelessHandle handle,
                               C_LinkWireless_Server servers[]) {
  LinkWireless::Server cppServers[C_LINK_WIRELESS_MAX_SERVERS];
  bool result = static_cast<LinkWireless*>(handle)->getServers(cppServers);

  for (u32 i = 0; i < C_LINK_WIRELESS_MAX_SERVERS; i++) {
    servers[i].id = cppServers[i].id;
    servers[i].gameId = cppServers[i].gameId;
    strncpy(servers[i].gameName, cppServers[i].gameName,
            C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH);
    servers[i].gameName[C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH] = '\0';
    strncpy(servers[i].userName, cppServers[i].userName,
            C_LINK_WIRELESS_MAX_USER_NAME_LENGTH);
    servers[i].userName[C_LINK_WIRELESS_MAX_USER_NAME_LENGTH] = '\0';
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
    strncpy(servers[i].gameName, cppServers[i].gameName,
            C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH);
    servers[i].gameName[C_LINK_WIRELESS_MAX_GAME_NAME_LENGTH] = '\0';
    strncpy(servers[i].userName, cppServers[i].userName,
            C_LINK_WIRELESS_MAX_USER_NAME_LENGTH);
    servers[i].userName[C_LINK_WIRELESS_MAX_USER_NAME_LENGTH] = '\0';
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

u8 C_LinkWireless_playerCount(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->playerCount();
}

u8 C_LinkWireless_currentPlayerId(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->currentPlayerId();
}

C_LinkWireless_Error C_LinkWireless_getLastError(C_LinkWirelessHandle handle,
                                                 bool clear) {
  return static_cast<C_LinkWireless_Error>(
      static_cast<LinkWireless*>(handle)->getLastError(clear));
}

bool C_LinkWireless_hasActiveAsyncCommand(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_hasActiveAsyncCommand();
}

bool C_LinkWireless_canSend(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_canSend();
}

u32 C_LinkWireless_getPendingCount(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_getPendingCount();
}

u32 C_LinkWireless_lastPacketId(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_lastPacketId();
}

u32 C_LinkWireless_lastConfirmationFromClient1(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_lastConfirmationFromClient1();
}

u32 C_LinkWireless_lastPacketIdFromClient1(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_lastPacketIdFromClient1();
}

u32 C_LinkWireless_lastConfirmationFromServer(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_lastConfirmationFromServer();
}

u32 C_LinkWireless_lastPacketIdFromServer(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_lastPacketIdFromServer();
}

u32 C_LinkWireless_nextPendingPacketId(C_LinkWirelessHandle handle) {
  return static_cast<LinkWireless*>(handle)->_nextPendingPacketId();
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

void C_LinkWireless_onACKTimer(C_LinkWirelessHandle handle) {
  static_cast<LinkWireless*>(handle)->_onACKTimer();
}
}
