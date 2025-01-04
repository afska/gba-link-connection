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
    C_LinkWirelessMultiboot_ListenerCallback listener) {
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
      });

  return static_cast<C_LinkWirelessMultiboot_Result>(result);
}

C_LinkWirelessMultiboot_Result
C_LinkWirelessMultiboot_sendRomAndKeepConnectionAlive(
    C_LinkWirelessMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    const char* gameName,
    const char* userName,
    u16 gameId,
    u8 players,
    C_LinkWirelessMultiboot_ListenerCallback listener) {
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
      true);

  return static_cast<C_LinkWirelessMultiboot_Result>(result);
}

bool C_LinkWirelessMultiboot_reset(C_LinkWirelessMultibootHandle handle) {
  return static_cast<LinkWirelessMultiboot*>(handle)->reset();
}
}
