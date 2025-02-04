#include "C_LinkCable.h"
#include "../LinkCable.hpp"

extern "C" {

C_LinkCableHandle C_LinkCable_createDefault() {
  return new LinkCable();
}

C_LinkCableHandle C_LinkCable_create(C_LinkCable_BaudRate baudRate,
                                     u32 timeout,
                                     u16 interval,
                                     u8 sendTimerId) {
  return new LinkCable(static_cast<LinkCable::BaudRate>(baudRate), timeout,
                       interval, sendTimerId);
}

void C_LinkCable_destroy(C_LinkCableHandle handle) {
  delete static_cast<LinkCable*>(handle);
}

bool C_LinkCable_isActive(C_LinkCableHandle handle) {
  return static_cast<LinkCable*>(handle)->isActive();
}

void C_LinkCable_activate(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->activate();
}

void C_LinkCable_deactivate(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->deactivate();
}

bool C_LinkCable_isConnected(C_LinkCableHandle handle) {
  return static_cast<LinkCable*>(handle)->isConnected();
}

u8 C_LinkCable_playerCount(C_LinkCableHandle handle) {
  return static_cast<LinkCable*>(handle)->playerCount();
}

u8 C_LinkCable_currentPlayerId(C_LinkCableHandle handle) {
  return static_cast<LinkCable*>(handle)->currentPlayerId();
}

void C_LinkCable_sync(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->sync();
}

bool C_LinkCable_waitFor(C_LinkCableHandle handle, u8 playerId) {
  return static_cast<LinkCable*>(handle)->waitFor(playerId);
}

bool C_LinkCable_waitForWithCancel(C_LinkCableHandle handle,
                                   u8 playerId,
                                   bool (*cancel)()) {
  return static_cast<LinkCable*>(handle)->waitFor(playerId, cancel);
}

bool C_LinkCable_canRead(C_LinkCableHandle handle, u8 playerId) {
  return static_cast<LinkCable*>(handle)->canRead(playerId);
}

u16 C_LinkCable_read(C_LinkCableHandle handle, u8 playerId) {
  return static_cast<LinkCable*>(handle)->read(playerId);
}

u16 C_LinkCable_peek(C_LinkCableHandle handle, u8 playerId) {
  return static_cast<LinkCable*>(handle)->peek(playerId);
}

bool C_LinkCable_canSend(C_LinkCableHandle handle) {
  return static_cast<LinkCable*>(handle)->canSend();
}

bool C_LinkCable_send(C_LinkCableHandle handle, u16 data) {
  return static_cast<LinkCable*>(handle)->send(data);
}

bool C_LinkCable_didQueueOverflow(C_LinkCableHandle handle, bool clear) {
  return static_cast<LinkCable*>(handle)->didQueueOverflow(clear);
}

void C_LinkCable_resetTimeout(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->resetTimeout();
}

void C_LinkCable_resetTimer(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->resetTimer();
}

C_LinkCable_Config C_LinkCable_getConfig(C_LinkCableHandle handle) {
  C_LinkCable_Config config;
  auto instance = static_cast<LinkCable*>(handle);
  config.baudRate =
      static_cast<C_LinkCable_BaudRate>(instance->config.baudRate);
  config.timeout = instance->config.timeout;
  config.interval = instance->config.interval;
  config.sendTimerId = instance->config.sendTimerId;
  return config;
}

void C_LinkCable_setConfig(C_LinkCableHandle handle,
                           C_LinkCable_Config config) {
  auto instance = static_cast<LinkCable*>(handle);
  instance->config.baudRate = static_cast<LinkCable::BaudRate>(config.baudRate);
  instance->config.timeout = config.timeout;
  instance->config.interval = config.interval;
  instance->config.sendTimerId = config.sendTimerId;
}

void C_LinkCable_onVBlank(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->_onVBlank();
}

void C_LinkCable_onSerial(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->_onSerial();
}

void C_LinkCable_onTimer(C_LinkCableHandle handle) {
  static_cast<LinkCable*>(handle)->_onTimer();
}
}
