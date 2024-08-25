#include "C_LinkCableMultiboot.h"
#include "../LinkCableMultiboot.hpp"

extern "C" {

C_LinkCableMultibootHandle C_LinkCableMultiboot_create(void) {
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
}
