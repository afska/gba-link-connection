#include "C_LinkSPI.h"
#include "../LinkSPI.hpp"

extern "C" {

C_LinkSPIHandle C_LinkSPI_create() {
  return new LinkSPI();
}

void C_LinkSPI_destroy(C_LinkSPIHandle handle) {
  delete static_cast<LinkSPI*>(handle);
}

bool C_LinkSPI_isActive(C_LinkSPIHandle handle) {
  return static_cast<LinkSPI*>(handle)->isActive();
}

void C_LinkSPI_activate(C_LinkSPIHandle handle,
                        C_LinkSPI_Mode mode,
                        C_LinkSPI_DataSize dataSize) {
  static_cast<LinkSPI*>(handle)->activate(
      static_cast<LinkSPI::Mode>(mode),
      static_cast<LinkSPI::DataSize>(dataSize));
}

void C_LinkSPI_deactivate(C_LinkSPIHandle handle) {
  static_cast<LinkSPI*>(handle)->deactivate();
}

u32 C_LinkSPI_transfer(C_LinkSPIHandle handle, u32 data) {
  return static_cast<LinkSPI*>(handle)->transfer(data);
}

u32 C_LinkSPI_transferWithCancel(C_LinkSPIHandle handle,
                                 u32 data,
                                 bool (*cancel)()) {
  return static_cast<LinkSPI*>(handle)->transfer(data, cancel);
}

void C_LinkSPI_transferAsync(C_LinkSPIHandle handle, u32 data) {
  static_cast<LinkSPI*>(handle)->transferAsync(data);
}

void C_LinkSPI_transferAsyncWithCancel(C_LinkSPIHandle handle,
                                       u32 data,
                                       bool (*cancel)()) {
  static_cast<LinkSPI*>(handle)->transferAsync(data, cancel);
}

C_LinkSPI_AsyncState C_LinkSPI_getAsyncState(C_LinkSPIHandle handle) {
  return static_cast<C_LinkSPI_AsyncState>(
      static_cast<LinkSPI*>(handle)->getAsyncState());
}

u32 C_LinkSPI_getAsyncData(C_LinkSPIHandle handle) {
  return static_cast<LinkSPI*>(handle)->getAsyncData();
}

C_LinkSPI_Mode C_LinkSPI_getMode(C_LinkSPIHandle handle) {
  return static_cast<C_LinkSPI_Mode>(static_cast<LinkSPI*>(handle)->getMode());
}

C_LinkSPI_DataSize C_LinkSPI_getDataSize(C_LinkSPIHandle handle) {
  return static_cast<C_LinkSPI_DataSize>(
      static_cast<LinkSPI*>(handle)->getDataSize());
}

void C_LinkSPI_setWaitModeActive(C_LinkSPIHandle handle, bool isActive) {
  static_cast<LinkSPI*>(handle)->setWaitModeActive(isActive);
}

bool C_LinkSPI_isWaitModeActive(C_LinkSPIHandle handle) {
  return static_cast<LinkSPI*>(handle)->isWaitModeActive();
}

void C_LinkSPI_onSerial(C_LinkSPIHandle handle, bool customAck) {
  static_cast<LinkSPI*>(handle)->_onSerial(customAck);
}
}
