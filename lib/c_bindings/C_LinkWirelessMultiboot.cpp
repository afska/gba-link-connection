#include "C_LinkWirelessMultiboot.h"
#include "../LinkWirelessMultiboot.hpp"

extern "C" {

C_LinkWirelessMultibootHandle C_LinkWirelessMultiboot_create() {
  return new LinkWirelessMultiboot();
}

void C_LinkWirelessMultiboot_destroy(C_LinkWirelessMultibootHandle handle) {
  delete static_cast<LinkWirelessMultiboot*>(handle);
}

C_LinkWirelessMultiboot_Result C_LinkWirelessMultiboot_sendRom(
    C_LinkWirelessMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    const char* gameName,
    const char* userName,
    u16 gameId,
    u8 players,
    C_LinkWirelessMultiboot_ListenerCallback listener,
    bool keepConnectionAlive) {
  auto result = static_cast<LinkWirelessMultiboot*>(handle)->sendRom(
      rom, romSize, gameName, userName, gameId, players,
      [listener](LinkWirelessMultiboot::MultibootProgress progress) {
        C_LinkWirelessMultiboot_Progress cProgress;
        cProgress.state =
            static_cast<C_LinkWirelessMultiboot_State>(progress.state);
        cProgress.connectedClients = progress.connectedClients;
        cProgress.percentage = progress.percentage;
        cProgress.ready = progress.ready;
        return listener(cProgress);
      },
      keepConnectionAlive);

  return static_cast<C_LinkWirelessMultiboot_Result>(result);
}

bool C_LinkWirelessMultiboot_reset(C_LinkWirelessMultibootHandle handle) {
  return static_cast<LinkWirelessMultiboot*>(handle)->reset();
}

C_LinkWirelessMultiboot_AsyncHandle C_LinkWirelessMultiboot_Async_create() {
  return new LinkWirelessMultiboot::Async();
}

void C_LinkWirelessMultiboot_Async_destroy(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  delete static_cast<LinkWirelessMultiboot::Async*>(handle);
}

bool C_LinkWirelessMultiboot_Async_sendRom(C_LinkWirelessMultibootHandle handle,
                                           const u8* rom,
                                           u32 romSize,
                                           const char* gameName,
                                           const char* userName,
                                           u16 gameId,
                                           u8 players,
                                           bool waitForReadySignal,
                                           bool keepConnectionAlive,
                                           u32 maxTransfersPerFrame) {
  return static_cast<LinkWirelessMultiboot::Async*>(handle)->sendRom(
      rom, romSize, gameName, userName, gameId, players, waitForReadySignal,
      keepConnectionAlive, maxTransfersPerFrame);
}

bool C_LinkWirelessMultiboot_Async_reset(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  return static_cast<LinkWirelessMultiboot::Async*>(handle)->reset();
}

C_LinkWirelessMultiboot_Async_State C_LinkWirelessMultiboot_Async_getState(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  return static_cast<C_LinkWirelessMultiboot_Async_State>(
      static_cast<LinkWirelessMultiboot::Async*>(handle)->getState());
}

C_LinkWirelessMultiboot_Async_Result C_LinkWirelessMultiboot_Async_getResult(
    C_LinkWirelessMultiboot_AsyncHandle handle,
    bool clear) {
  return static_cast<C_LinkWirelessMultiboot_Async_Result>(
      static_cast<LinkWirelessMultiboot::Async*>(handle)->getResult(clear));
}

u8 C_LinkWirelessMultiboot_Async_playerCount(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  return static_cast<LinkWirelessMultiboot::Async*>(handle)->playerCount();
}

u8 C_LinkWirelessMultiboot_Async_getPercentage(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  return static_cast<LinkWirelessMultiboot::Async*>(handle)->getPercentage();
}

bool C_LinkWirelessMultiboot_Async_isReady(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  return static_cast<LinkWirelessMultiboot::Async*>(handle)->isReady();
}

void C_LinkWirelessMultiboot_Async_markReady(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  static_cast<LinkWirelessMultiboot::Async*>(handle)->markReady();
}

void C_LinkWirelessMultiboot_Async_onVBlank(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  static_cast<LinkWirelessMultiboot::Async*>(handle)->_onVBlank();
}

void C_LinkWirelessMultiboot_Async_onSerial(
    C_LinkWirelessMultiboot_AsyncHandle handle) {
  static_cast<LinkWirelessMultiboot::Async*>(handle)->_onSerial();
}
}
