#include "C_LinkUniversal.h"
#include "../LinkUniversal.hpp"

extern "C" {

C_LinkUniversalHandle C_LinkUniversal_create(
    C_LinkUniversal_Protocol protocol,
    const char* gameName,
    C_LinkUniversal_CableOptions cableOptions,
    C_LinkUniversal_WirelessOptions wirelessOptions,
    int randomSeed) {
  return new LinkUniversal(
      static_cast<LinkUniversal::Protocol>(protocol), gameName,
      {static_cast<LinkCable::BaudRate>(cableOptions.baudRate),
       cableOptions.timeout, cableOptions.interval, cableOptions.sendTimerId},
      {wirelessOptions.retransmission, wirelessOptions.maxPlayers,
       wirelessOptions.timeout, wirelessOptions.interval,
       wirelessOptions.sendTimerId, wirelessOptions.asyncACKTimerId},
      randomSeed);
}

void C_LinkUniversal_destroy(C_LinkUniversalHandle handle) {
  delete static_cast<LinkUniversal*>(handle);
}

bool C_LinkUniversal_isActive(C_LinkUniversalHandle handle) {
  return static_cast<LinkUniversal*>(handle)->isActive();
}

void C_LinkUniversal_activate(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->activate();
}

void C_LinkUniversal_deactivate(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->deactivate();
}

bool C_LinkUniversal_isConnected(C_LinkUniversalHandle handle) {
  return static_cast<LinkUniversal*>(handle)->isConnected();
}

uint8_t C_LinkUniversal_playerCount(C_LinkUniversalHandle handle) {
  return static_cast<LinkUniversal*>(handle)->playerCount();
}

uint8_t C_LinkUniversal_currentPlayerId(C_LinkUniversalHandle handle) {
  return static_cast<LinkUniversal*>(handle)->currentPlayerId();
}

void C_LinkUniversal_sync(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->sync();
}

bool C_LinkUniversal_waitFor(C_LinkUniversalHandle handle, uint8_t playerId) {
  return static_cast<LinkUniversal*>(handle)->waitFor(playerId);
}

bool C_LinkUniversal_waitForWithCancel(C_LinkUniversalHandle handle,
                                       uint8_t playerId,
                                       bool (*cancel)()) {
  return static_cast<LinkUniversal*>(handle)->waitFor(playerId, cancel);
}

bool C_LinkUniversal_canRead(C_LinkUniversalHandle handle, uint8_t playerId) {
  return static_cast<LinkUniversal*>(handle)->canRead(playerId);
}

uint16_t C_LinkUniversal_read(C_LinkUniversalHandle handle, uint8_t playerId) {
  return static_cast<LinkUniversal*>(handle)->read(playerId);
}

uint16_t C_LinkUniversal_peek(C_LinkUniversalHandle handle, uint8_t playerId) {
  return static_cast<LinkUniversal*>(handle)->peek(playerId);
}

bool C_LinkUniversal_send(C_LinkUniversalHandle handle, uint16_t data) {
  return static_cast<LinkUniversal*>(handle)->send(data);
}

C_LinkUniversal_State C_LinkUniversal_getState(C_LinkUniversalHandle handle) {
  return static_cast<C_LinkUniversal_State>(
      static_cast<LinkUniversal*>(handle)->getState());
}

C_LinkUniversal_Mode C_LinkUniversal_getMode(C_LinkUniversalHandle handle) {
  return static_cast<C_LinkUniversal_Mode>(
      static_cast<LinkUniversal*>(handle)->getMode());
}

C_LinkUniversal_Protocol C_LinkUniversal_getProtocol(
    C_LinkUniversalHandle handle) {
  return static_cast<C_LinkUniversal_Protocol>(
      static_cast<LinkUniversal*>(handle)->getProtocol());
}

void C_LinkUniversal_setProtocol(C_LinkUniversalHandle handle,
                                 C_LinkUniversal_Protocol protocol) {
  static_cast<LinkUniversal*>(handle)->setProtocol(
      static_cast<LinkUniversal::Protocol>(protocol));
}

uint32_t C_LinkUniversal_getWaitCount(C_LinkUniversalHandle handle) {
  return static_cast<LinkUniversal*>(handle)->_getWaitCount();
}

uint32_t C_LinkUniversal_getSubWaitCount(C_LinkUniversalHandle handle) {
  return static_cast<LinkUniversal*>(handle)->_getSubWaitCount();
}

void C_LinkUniversal_onVBlank(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->_onVBlank();
}

void C_LinkUniversal_onSerial(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->_onSerial();
}

void C_LinkUniversal_onTimer(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->_onTimer();
}

void C_LinkUniversal_onACKTimer(C_LinkUniversalHandle handle) {
  static_cast<LinkUniversal*>(handle)->_onACKTimer();
}
}