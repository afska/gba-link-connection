#include "C_LinkCableMultiboot.h"
#include "../LinkCableMultiboot.hpp"

extern "C" {

C_LinkCableMultibootHandle C_LinkCableMultiboot_create() {
  return new LinkCableMultiboot();
}

void C_LinkCableMultiboot_destroy(C_LinkCableMultibootHandle handle) {
  delete static_cast<LinkCableMultiboot*>(handle);
}

C_LinkCableMultiboot_Result C_LinkCableMultiboot_sendRom(
    C_LinkCableMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    bool (*cancel)(void),
    C_LinkCableMultiboot_TransferMode mode) {
  return static_cast<C_LinkCableMultiboot_Result>(
      static_cast<LinkCableMultiboot*>(handle)->sendRom(
          rom, romSize, cancel,
          static_cast<LinkCableMultiboot::TransferMode>(mode)));
}

C_LinkCableMultiboot_AsyncHandle C_LinkCableMultiboot_Async_create() {
  return new LinkCableMultiboot::Async();
}

void C_LinkCableMultiboot_Async_destroy(
    C_LinkCableMultiboot_AsyncHandle handle) {
  delete static_cast<LinkCableMultiboot::Async*>(handle);
}

bool C_LinkCableMultiboot_Async_sendRom(
    C_LinkCableMultiboot_AsyncHandle handle,
    const u8* rom,
    u32 romSize,
    bool waitForReadySignal,
    C_LinkCableMultiboot_TransferMode mode) {
  return static_cast<LinkCableMultiboot::Async*>(handle)->sendRom(
      rom, romSize, waitForReadySignal,
      static_cast<LinkCableMultiboot::TransferMode>(mode));
}

void C_LinkCableMultiboot_Async_reset(C_LinkCableMultiboot_AsyncHandle handle) {
  static_cast<LinkCableMultiboot::Async*>(handle)->reset();
}

C_LinkCableMultiboot_Async_State C_LinkCableMultiboot_Async_getState(
    C_LinkCableMultiboot_AsyncHandle handle) {
  return static_cast<C_LinkCableMultiboot_Async_State>(
      static_cast<LinkCableMultiboot::Async*>(handle)->getState());
}

C_LinkCableMultiboot_Async_Result C_LinkCableMultiboot_Async_getResult(
    C_LinkCableMultiboot_AsyncHandle handle,
    bool clear) {
  return static_cast<C_LinkCableMultiboot_Async_Result>(
      static_cast<LinkCableMultiboot::Async*>(handle)->getResult(clear));
}

u8 C_LinkCableMultiboot_Async_playerCount(
    C_LinkCableMultiboot_AsyncHandle handle) {
  return static_cast<LinkCableMultiboot::Async*>(handle)->playerCount();
}

u8 C_LinkCableMultiboot_Async_getPercentage(
    C_LinkCableMultiboot_AsyncHandle handle) {
  return static_cast<LinkCableMultiboot::Async*>(handle)->getPercentage();
}

bool C_LinkCableMultiboot_Async_isReady(
    C_LinkCableMultiboot_AsyncHandle handle) {
  return static_cast<LinkCableMultiboot::Async*>(handle)->isReady();
}

void C_LinkCableMultiboot_Async_markReady(
    C_LinkCableMultiboot_AsyncHandle handle) {
  static_cast<LinkCableMultiboot::Async*>(handle)->markReady();
}

void C_LinkCableMultiboot_Async_onVBlank(
    C_LinkCableMultiboot_AsyncHandle handle) {
  static_cast<LinkCableMultiboot::Async*>(handle)->_onVBlank();
}

void C_LinkCableMultiboot_Async_onSerial(
    C_LinkCableMultiboot_AsyncHandle handle) {
  static_cast<LinkCableMultiboot::Async*>(handle)->_onSerial();
}
}
