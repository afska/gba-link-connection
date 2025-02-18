#include "C_LinkUART.h"
#include "../LinkUART.hpp"

extern "C" {
C_LinkUARTHandle C_LinkUART_create() {
  return new LinkUART();
}

void C_LinkUART_destroy(C_LinkUARTHandle handle) {
  delete static_cast<LinkUART*>(handle);
}

bool C_LinkUART_isActive(C_LinkUARTHandle handle) {
  return static_cast<LinkUART*>(handle)->isActive();
}

void C_LinkUART_activate(C_LinkUARTHandle handle,
                         C_LinkUART_BaudRate baudRate,
                         C_LinkUART_DataSize dataSize,
                         C_LinkUART_Parity parity,
                         bool useCTS) {
  static_cast<LinkUART*>(handle)->activate(
      static_cast<LinkUART::BaudRate>(baudRate),
      static_cast<LinkUART::DataSize>(dataSize),
      static_cast<LinkUART::Parity>(parity), useCTS);
}

void C_LinkUART_deactivate(C_LinkUARTHandle handle) {
  static_cast<LinkUART*>(handle)->deactivate();
}

void C_LinkUART_sendLine(C_LinkUARTHandle handle, const char* string) {
  static_cast<LinkUART*>(handle)->sendLine(string);
}

void C_LinkUART_sendLineWithCancel(C_LinkUARTHandle handle,
                                   const char* string,
                                   bool (*cancel)()) {
  static_cast<LinkUART*>(handle)->sendLine(string, cancel);
}

bool C_LinkUART_readLine(C_LinkUARTHandle handle, char* string, u32 limit) {
  return static_cast<LinkUART*>(handle)->readLine(string, limit);
}

bool C_LinkUART_readLineWithCancel(C_LinkUARTHandle handle,
                                   char* string,
                                   bool (*cancel)(),
                                   u32 limit) {
  return static_cast<LinkUART*>(handle)->readLine(string, cancel, limit);
}

void C_LinkUART_send(C_LinkUARTHandle handle,
                     const u8* buffer,
                     u32 size,
                     u32 offset) {
  static_cast<LinkUART*>(handle)->send(buffer, size, offset);
}

u32 C_LinkUART_read(C_LinkUARTHandle handle, u8* buffer, u32 size, u32 offset) {
  return static_cast<LinkUART*>(handle)->read(buffer, size, offset);
}

bool C_LinkUART_canRead(C_LinkUARTHandle handle) {
  return static_cast<LinkUART*>(handle)->canRead();
}

bool C_LinkUART_canSend(C_LinkUARTHandle handle) {
  return static_cast<LinkUART*>(handle)->canSend();
}

u32 C_LinkUART_availableForRead(C_LinkUARTHandle handle) {
  return static_cast<LinkUART*>(handle)->availableForRead();
}

u32 C_LinkUART_availableForSend(C_LinkUARTHandle handle) {
  return static_cast<LinkUART*>(handle)->availableForSend();
}

u8 C_LinkUART_readByte(C_LinkUARTHandle handle) {
  return static_cast<LinkUART*>(handle)->read();
}

void C_LinkUART_sendByte(C_LinkUARTHandle handle, u8 data) {
  static_cast<LinkUART*>(handle)->send(data);
}

void C_LinkUART_onSerial(C_LinkUARTHandle handle) {
  static_cast<LinkUART*>(handle)->_onSerial();
}
}
