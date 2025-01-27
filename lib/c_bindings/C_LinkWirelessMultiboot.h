#ifndef C_BINDINGS_LINK_WIRELESS_MULTIBOOT_H
#define C_BINDINGS_LINK_WIRELESS_MULTIBOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkWirelessMultibootHandle;
typedef void* C_LinkWirelessMultiboot_AsyncHandle;

#define C_LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xC0)
#define C_LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define C_LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS 2
#define C_LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS 5

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_STATE_STOPPED,
  C_LINK_WIRELESS_MULTIBOOT_STATE_INITIALIZING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_WAITING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_PREPARING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_SENDING,
  C_LINK_WIRELESS_MULTIBOOT_STATE_CONFIRMING
} C_LinkWirelessMultiboot_State;

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_RESULT_SUCCESS,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_INVALID_SIZE,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_INVALID_PLAYERS,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_CANCELED,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_ADAPTER_NOT_DETECTED,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_BAD_HANDSHAKE,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_CLIENT_DISCONNECTED,
  C_LINK_WIRELESS_MULTIBOOT_RESULT_FAILURE
} C_LinkWirelessMultiboot_Result;

typedef struct {
  C_LinkWirelessMultiboot_State state;
  u8 connectedClients;
  u8 percentage;
  volatile bool* ready;
} C_LinkWirelessMultiboot_Progress;

typedef bool (*C_LinkWirelessMultiboot_ListenerCallback)(
    C_LinkWirelessMultiboot_Progress progress);

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_STOPPED,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_INITIALIZING,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_STARTING,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_LISTENING,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_HANDSHAKING_CLIENT_STEP1,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_HANDSHAKING_CLIENT_STEP2,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_HANDSHAKING_CLIENT_STEP3,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_HANDSHAKING_CLIENT_STEP4,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_HANDSHAKING_CLIENT_STEP5,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_ENDING_HOST,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_SENDING_ROM_START_COMMAND,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_RESTING,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_ENSURING_CLIENTS_ALIVE,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_SENDING_ROM_PART,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_CONFIRMING_STEP1,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_STATE_CONFIRMING_STEP2,
} C_LinkWirelessMultiboot_Async_State;

typedef enum {
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_NONE = -1,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_SUCCESS = 0,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_INVALID_SIZE = 1,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_INVALID_PLAYERS = 2,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_ADAPTER_NOT_DETECTED = 3,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_INIT_FAILURE = 4,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_BAD_HANDSHAKE = 5,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_CLIENT_DISCONNECTED = 6,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_FAILURE = 7,
  C_LINK_WIRELESS_MULTIBOOT_ASYNC_RESULT_IRQ_TIMEOUT = 8
} C_LinkWirelessMultiboot_Async_Result;

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
    C_LinkWirelessMultiboot_ListenerCallback listener,
    bool keepConnectionAlive);

bool C_LinkWirelessMultiboot_reset(C_LinkWirelessMultibootHandle handle);

C_LinkWirelessMultiboot_AsyncHandle C_LinkWirelessMultiboot_Async_create();
void C_LinkWirelessMultiboot_Async_destroy(
    C_LinkWirelessMultiboot_AsyncHandle handle);

bool C_LinkWirelessMultiboot_Async_sendRom(C_LinkWirelessMultibootHandle handle,
                                           const u8* rom,
                                           u32 romSize,
                                           const char* gameName,
                                           const char* userName,
                                           u16 gameId,
                                           u8 players,
                                           bool waitForReadySignal,
                                           bool keepConnectionAlive,
                                           u32 maxTransfersPerFrame);

bool C_LinkWirelessMultiboot_Async_reset(
    C_LinkWirelessMultiboot_AsyncHandle handle);

C_LinkWirelessMultiboot_Async_State C_LinkWirelessMultiboot_Async_getState(
    C_LinkWirelessMultiboot_AsyncHandle handle);
C_LinkWirelessMultiboot_Async_Result C_LinkWirelessMultiboot_Async_getResult(
    C_LinkWirelessMultiboot_AsyncHandle handle,
    bool clear);

u8 C_LinkWirelessMultiboot_Async_playerCount(
    C_LinkWirelessMultiboot_AsyncHandle handle);
u8 C_LinkWirelessMultiboot_Async_getPercentage(
    C_LinkWirelessMultiboot_AsyncHandle handle);

bool C_LinkWirelessMultiboot_Async_isReady(
    C_LinkWirelessMultiboot_AsyncHandle handle);
void C_LinkWirelessMultiboot_Async_markReady(
    C_LinkWirelessMultiboot_AsyncHandle handle);

void C_LinkWirelessMultiboot_Async_onVBlank(
    C_LinkWirelessMultiboot_AsyncHandle handle);
void C_LinkWirelessMultiboot_Async_onSerial(
    C_LinkWirelessMultiboot_AsyncHandle handle);

extern C_LinkWirelessMultibootHandle cLinkWirelessMultiboot;
extern C_LinkWirelessMultiboot_AsyncHandle cLinkWirelessMultibootAsync;

inline void C_LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_VBLANK() {
  C_LinkWirelessMultiboot_Async_onVBlank(cLinkWirelessMultibootAsync);
}

inline void C_LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_SERIAL() {
  C_LinkWirelessMultiboot_Async_onSerial(cLinkWirelessMultibootAsync);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_WIRELESS_MULTIBOOT_H
