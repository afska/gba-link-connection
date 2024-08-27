#ifndef C_BINDINGS_LINK_WIRELESS_MULTIBOOT_H
#define C_BINDINGS_LINK_WIRELESS_MULTIBOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkWirelessMultibootHandle;

#define C_LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xc0)
#define C_LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define C_LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS 2
#define C_LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS 5

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_SUCCESS,
  C_LINK_WIRELESS_MULTIBOOT_INVALID_SIZE,
  C_LINK_WIRELESS_MULTIBOOT_INVALID_PLAYERS,
  C_LINK_WIRELESS_MULTIBOOT_CANCELED,
  C_LINK_WIRELESS_MULTIBOOT_ADAPTER_NOT_DETECTED,
  C_LINK_WIRELESS_MULTIBOOT_BAD_HANDSHAKE,
  C_LINK_WIRELESS_MULTIBOOT_CLIENT_DISCONNECTED,
  C_LINK_WIRELESS_MULTIBOOT_FAILURE
} C_LinkWirelessMultiboot_Result;

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_STATE_STOPPED,
  C_LINK_WIRELESS_MULTIBOOT_STATE_INITIALIZING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_WAITING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_PREPARING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_SENDING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_CONFIRMING
} C_LinkWirelessMultiboot_State;

typedef struct {
  C_LinkWirelessMultiboot_State state;
  u32 connectedClients;
  u32 percentage;
} C_LinkWirelessMultiboot_Progress;

typedef bool (*C_LinkWirelessMultiboot_CancelCallback)(
    C_LinkWirelessMultiboot_Progress progress);

C_LinkWirelessMultibootHandle C_LinkWirelessMultiboot_create();
void C_LinkWirelessMultiboot_destroy(C_LinkWirelessMultibootHandle handle);

C_LinkWirelessMultiboot_Result C_LinkWirelessMultiboot_sendRom(
    C_LinkWirelessMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    const char* gameName,
    const char* userName,
    u16 gameId,
    u8 players,
    C_LinkWirelessMultiboot_CancelCallback cancel);

extern C_LinkWirelessMultibootHandle cLinkWirelessMultiboot;

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_WIRELESS_MULTIBOOT_H
