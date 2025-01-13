#include "C_LinkRawCable.h"
#include "../LinkRawCable.hpp"

extern "C" {

C_LinkRawCableHandle C_LinkRawCable_create() {
  return new LinkRawCable();
}

void C_LinkRawCable_destroy(C_LinkRawCableHandle handle) {
  delete static_cast<LinkRawCable*>(handle);
}

bool C_LinkRawCable_isActive(C_LinkRawCableHandle handle) {
  return static_cast<LinkRawCable*>(handle)->isActive();
}

void C_LinkRawCable_activate(C_LinkRawCableHandle handle,
                             C_LinkRawCable_BaudRate baudRate) {
  static_cast<LinkRawCable*>(handle)->activate(
      static_cast<LinkRawCable::BaudRate>(baudRate));
}

void C_LinkRawCable_deactivate(C_LinkRawCableHandle handle) {
  static_cast<LinkRawCable*>(handle)->deactivate();
}

C_LinkRawCable_Response C_LinkRawCable_transfer(C_LinkRawCableHandle handle,
                                                u16 data) {
  auto response = static_cast<LinkRawCable*>(handle)->transfer(data);
  C_LinkRawCable_Response cResponse;
  for (u32 i = 0; i < C_LINK_RAW_CABLE_MAX_PLAYERS; i++)
    cResponse.data[i] = response.data[i];
  cResponse.playerId = response.playerId;
  return cResponse;
}

C_LinkRawCable_Response C_LinkRawCable_transferWithCancel(
    C_LinkRawCableHandle handle,
    u16 data,
    bool (*cancel)()) {
  auto response = static_cast<LinkRawCable*>(handle)->transfer(data, cancel);
  C_LinkRawCable_Response cResponse;
  for (u32 i = 0; i < C_LINK_RAW_CABLE_MAX_PLAYERS; i++)
    cResponse.data[i] = response.data[i];
  cResponse.playerId = response.playerId;
  return cResponse;
}

void C_LinkRawCable_transferAsync(C_LinkRawCableHandle handle, u16 data) {
  static_cast<LinkRawCable*>(handle)->transferAsync(data);
}

C_LinkRawCable_AsyncState C_LinkRawCable_getAsyncState(
    C_LinkRawCableHandle handle) {
  return static_cast<C_LinkRawCable_AsyncState>(
      static_cast<LinkRawCable*>(handle)->getAsyncState());
}

C_LinkRawCable_Response C_LinkRawCable_getAsyncData(
    C_LinkRawCableHandle handle) {
  auto response = static_cast<LinkRawCable*>(handle)->getAsyncData();
  C_LinkRawCable_Response cResponse;
  for (u32 i = 0; i < C_LINK_RAW_CABLE_MAX_PLAYERS; i++)
    cResponse.data[i] = response.data[i];
  cResponse.playerId = response.playerId;
  return cResponse;
}

C_LinkRawCable_BaudRate C_LinkRawCable_getBaudRate(
    C_LinkRawCableHandle handle) {
  return static_cast<C_LinkRawCable_BaudRate>(
      static_cast<LinkRawCable*>(handle)->getBaudRate());
}

bool C_LinkRawCable_isMaster(C_LinkRawCableHandle handle) {
  return static_cast<LinkRawCable*>(handle)->isMaster();
}

bool C_LinkRawCable_isReady(C_LinkRawCableHandle handle) {
  return static_cast<LinkRawCable*>(handle)->isReady();
}

void C_LinkRawCable_onSerial(C_LinkRawCableHandle handle) {
  static_cast<LinkRawCable*>(handle)->_onSerial();
}
}
