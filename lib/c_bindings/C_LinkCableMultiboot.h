#ifndef C_BINDINGS_LINK_CABLE_MULTIBOOT_H
#define C_BINDINGS_LINK_CABLE_MULTIBOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef enum {
  C_LINK_CABLE_MULTIBOOT_SUCCESS,
  C_LINK_CABLE_MULTIBOOT_INVALID_SIZE,
  C_LINK_CABLE_MULTIBOOT_CANCELED,
  C_LINK_CABLE_MULTIBOOT_FAILURE_DURING_TRANSFER
} C_LinkCableMultiboot_Result;

typedef enum {
  C_LINK_CABLE_MULTIBOOT_TRANSFER_MODE_SPI = 0,
  C_LINK_CABLE_MULTIBOOT_TRANSFER_MODE_MULTI_PLAY = 1
} C_LinkCableMultiboot_TransferMode;

typedef void* C_LinkCableMultibootHandle;

C_LinkCableMultibootHandle C_LinkCableMultiboot_create(void);
void C_LinkCableMultiboot_destroy(C_LinkCableMultibootHandle handle);

C_LinkCableMultiboot_Result C_LinkCableMultiboot_sendRom(
    C_LinkCableMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    bool (*cancel)(void),
    C_LinkCableMultiboot_TransferMode mode);

extern C_LinkCableMultibootHandle cLinkCableMultiboot;

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_CABLE_MULTIBOOT_H
