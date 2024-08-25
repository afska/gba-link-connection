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
    C_LinkWirelessMultiboot_CancelCallback cancel) {
  auto result = static_cast<LinkWirelessMultiboot*>(handle)->sendRom(
      rom, romSize, gameName, userName, gameId, players,
      [cancel](LinkWirelessMultiboot::MultibootProgress progress) {
        C_LinkWirelessMultiboot_Progress cProgress;
        cProgress.state =
            static_cast<C_LinkWirelessMultiboot_State>(progress.state);
        cProgress.connectedClients = progress.connectedClients;
        cProgress.percentage = progress.percentage;
        return cancel(cProgress);
      });

  return static_cast<C_LinkWirelessMultiboot_Result>(result);
}
}
