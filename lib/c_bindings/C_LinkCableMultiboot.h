#ifndef C_BINDINGS_LINK_CABLE_MULTIBOOT_H
#define C_BINDINGS_LINK_CABLE_MULTIBOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef enum {
  C_LINK_CABLE_MULTIBOOT_RESULT_SUCCESS,
  C_LINK_CABLE_MULTIBOOT_RESULT_UNALIGNED,
  C_LINK_CABLE_MULTIBOOT_RESULT_INVALID_SIZE,
  C_LINK_CABLE_MULTIBOOT_RESULT_CANCELED,
  C_LINK_CABLE_MULTIBOOT_RESULT_FAILURE_DURING_TRANSFER
} C_LinkCableMultiboot_Result;

typedef enum {
  C_LINK_CABLE_MULTIBOOT_TRANSFER_MODE_SPI = 0,
  C_LINK_CABLE_MULTIBOOT_TRANSFER_MODE_MULTI_PLAY = 1
} C_LinkCableMultiboot_TransferMode;

typedef enum {
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_STOPPED = 0,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_WAITING = 1,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_DETECTING_CLIENTS = 2,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_DETECTING_CLIENTS_END = 3,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_HEADER = 4,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_PALETTE = 5,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_CONFIRMING_HANDSHAKE_DATA = 6,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_WAITING_BEFORE_MAIN_TRANSFER = 7,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_CALCULATING_CRCB = 8,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_ROM = 9,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_ROM_END = 10,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_ROM_END_WAITING = 11,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_FINAL_CRC = 12,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_CHECKING_FINAL_CRC = 13
} C_LinkCableMultiboot_Async_State;

typedef enum {
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_NONE = -1,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_SUCCESS = 0,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_UNALIGNED = 1,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_INVALID_SIZE = 2,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_SEND_FAILURE = 3,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_FINAL_HANDSHAKE_FAILURE = 4,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_CRC_FAILURE = 5,
} Result;

typedef void* C_LinkCableMultibootHandle;
typedef void* C_LinkCableMultiboot_AsyncHandle;

C_LinkCableMultibootHandle C_LinkCableMultiboot_create();
void C_LinkCableMultiboot_destroy(C_LinkCableMultibootHandle handle);

C_LinkCableMultiboot_Result C_LinkCableMultiboot_sendRom(
    C_LinkCableMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    bool (*cancel)(void),
    C_LinkCableMultiboot_TransferMode mode);

C_LinkCableMultiboot_AsyncHandle C_LinkCableMultiboot_Async_create();
void C_LinkCableMultiboot_Async_destroy(
    C_LinkCableMultiboot_AsyncHandle handle);

bool C_LinkCableMultiboot_Async_sendRom(C_LinkCableMultiboot_AsyncHandle handle,
                                        const u8* rom,
                                        u32 romSize,
                                        bool waitForReadySignal,
                                        C_LinkCableMultiboot_TransferMode mode);

void C_LinkCableMultiboot_Async_reset(C_LinkCableMultiboot_AsyncHandle handle);

C_LinkCableMultiboot_Async_State C_LinkCableMultiboot_Async_getState(
    C_LinkCableMultiboot_AsyncHandle handle);
void C_LinkCableMultiboot_Async_getResult(
    C_LinkCableMultiboot_AsyncHandle handle,
    bool clear);

u8 C_LinkCableMultiboot_Async_playerCount(
    C_LinkCableMultiboot_AsyncHandle handle);
u8 C_LinkCableMultiboot_Async_getPercentage(
    C_LinkCableMultiboot_AsyncHandle handle);

bool C_LinkCableMultiboot_Async_isReady(
    C_LinkCableMultiboot_AsyncHandle handle);
void C_LinkCableMultiboot_Async_markReady(
    C_LinkCableMultiboot_AsyncHandle handle);

void C_LinkCableMultiboot_Async_onVBlank(
    C_LinkCableMultiboot_AsyncHandle handle);
void C_LinkCableMultiboot_Async_onSerial(
    C_LinkCableMultiboot_AsyncHandle handle);

extern C_LinkCableMultibootHandle cLinkCableMultiboot;
extern C_LinkCableMultiboot_AsyncHandle cLinkCableMultibootAsync;

inline void C_LINK_CABLE_MULTIBOOT_ASYNC_ISR_VBLANK() {
  C_LinkCableMultiboot_Async_onVBlank(cLinkCableMultibootAsync);
}

inline void C_LINK_CABLE_MULTIBOOT_ASYNC_ISR_SERIAL() {
  C_LinkCableMultiboot_Async_onSerial(cLinkCableMultibootAsync);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_CABLE_MULTIBOOT_H
