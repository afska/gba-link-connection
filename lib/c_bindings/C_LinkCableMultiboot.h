#ifndef C_BINDINGS_LINK_CABLE_MULTIBOOT_H
#define C_BINDINGS_LINK_CABLE_MULTIBOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkCableMultibootHandle;
typedef void* C_LinkCableMultiboot_AsyncHandle;

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

typedef struct {
  bool waitForReadySignal;
  C_LinkCableMultiboot_TransferMode mode;
} C_LinkCableMultiboot_Async_Config;

typedef enum {
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_STOPPED,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_WAITING,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_DETECTING_CLIENTS,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_DETECTING_CLIENTS_END,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_HEADER,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_PALETTE,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_CONFIRMING_HANDSHAKE_DATA,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_WAITING_BEFORE_MAIN_TRANSFER,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_CALCULATING_CRCB,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_ROM,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_ROM_END,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_ROM_END_WAITING,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_SENDING_FINAL_CRC,
  C_LINK_CABLE_MULTIBOOT_ASYNC_STATE_CHECKING_FINAL_CRC
} C_LinkCableMultiboot_Async_State;

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_GENERAL_RESULT_NONE = -1,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_GENERAL_RESULT_SUCCESS = 0,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_GENERAL_RESULT_INVALID_DATA = 1,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_GENERAL_RESULT_INIT_FAILED = 2,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_GENERAL_RESULT_FAILURE = 3,
} C_LinkCableMultiboot_Async_GeneralResult;

typedef enum {
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_NONE = -1,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_SUCCESS = 0,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_UNALIGNED = 1,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_INVALID_SIZE = 2,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_SEND_FAILURE = 3,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_FINAL_HANDSHAKE_FAILURE = 4,
  C_LINK_CABLE_MULTIBOOT_ASYNC_RESULT_CRC_FAILURE = 5,
} C_LinkCableMultiboot_Async_Result;

C_LinkCableMultibootHandle C_LinkCableMultiboot_create();
void C_LinkCableMultiboot_destroy(C_LinkCableMultibootHandle handle);

C_LinkCableMultiboot_Result C_LinkCableMultiboot_sendRom(
    C_LinkCableMultibootHandle handle,
    const u8* rom,
    u32 romSize,
    bool (*cancel)(void),
    C_LinkCableMultiboot_TransferMode mode);

C_LinkCableMultiboot_AsyncHandle C_LinkCableMultiboot_Async_createDefault();
C_LinkCableMultiboot_AsyncHandle C_LinkCableMultiboot_Async_create(
    bool waitForReadySignal,
    C_LinkCableMultiboot_TransferMode mode);
void C_LinkCableMultiboot_Async_destroy(
    C_LinkCableMultiboot_AsyncHandle handle);

bool C_LinkCableMultiboot_Async_sendRom(C_LinkCableMultiboot_AsyncHandle handle,
                                        const u8* rom,
                                        u32 romSize);

bool C_LinkCableMultiboot_Async_reset(C_LinkCableMultiboot_AsyncHandle handle);

bool C_LinkCableMultiboot_Async_isSending(
    C_LinkCableMultiboot_AsyncHandle handle);
C_LinkCableMultiboot_Async_State C_LinkCableMultiboot_Async_getState(
    C_LinkCableMultiboot_AsyncHandle handle);
C_LinkCableMultiboot_Async_GeneralResult C_LinkCableMultiboot_Async_getResult(
    C_LinkCableMultiboot_AsyncHandle handle,
    bool clear);
C_LinkCableMultiboot_Async_Result C_LinkCableMultiboot_Async_getDetailedResult(
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

C_LinkCableMultiboot_Async_Config C_LinkCableMultiboot_Async_getConfig(
    C_LinkCableMultiboot_AsyncHandle handle);
void C_LinkCableMultiboot_Async_setConfig(
    C_LinkCableMultiboot_AsyncHandle handle,
    C_LinkCableMultiboot_Async_Config config);

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
