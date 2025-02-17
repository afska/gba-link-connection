#ifndef LINK_H
#define LINK_H

#include "def.h"

#define MEM_IO 0x04000000
#define REG_BASE MEM_IO
#define REG_RCNT *(vu16*)(REG_BASE + 0x0134)
#define REG_SIOCNT *(vu16*)(REG_BASE + 0x0128)
#define REG_SIOMLT_RECV *(vu16*)(REG_BASE + 0x0120)
#define REG_SIOMLT_SEND *(vu16*)(REG_BASE + 0x012A)
#define REG_SIOMULTI0 *(vu16*)(REG_BASE + 0x0120)
#define REG_IME *(vu16*)(REG_BASE + 0x0208)
#define REG_IF *(vu16*)(REG_BASE + 0x0202)
#define IRQ_SERIAL 0x0080

#define BITS_PLAYER_ID 4
#define BIT_READY 3
#define BIT_ERROR 6
#define BIT_START 7
#define BIT_MULTIPLAYER 13
#define BIT_IRQ 14
#define BIT_GENERAL_PURPOSE_HIGH 15

#define DISCONNECTED 0xFFFF

inline void setMultiPlayMode(int baudRate) {
  REG_RCNT = 0;
  REG_SIOCNT = (1 << BIT_MULTIPLAYER) | baudRate;
  REG_SIOMLT_SEND = 0;
}

static void setGeneralPurposeMode() {
  REG_RCNT = 1 << BIT_GENERAL_PURPOSE_HIGH;
  REG_SIOCNT = 0;
}

inline void setData(u16 data) {
  REG_SIOMLT_SEND = data;
}

inline u16 getDataFromPlayer0() {
  return REG_SIOMULTI0;
}

inline void startTransfer() {
  REG_SIOCNT |= 1 << BIT_START;
}

static void setInterruptsOn() {
  REG_SIOCNT |= 1 << BIT_IRQ;
}

inline bool wasTransferOk() {
  bool allReady = (REG_SIOCNT >> BIT_READY) & 1;
  bool hasError = (REG_SIOCNT >> BIT_ERROR) & 1;
  return allReady && !hasError;
}

inline u32 assignedPlayerId() {
  return (REG_SIOCNT & (0b11 << BITS_PLAYER_ID)) >> BITS_PLAYER_ID;
}

inline bool didSerialInterruptOccur() {
  return REG_IF & IRQ_SERIAL;
}

inline void acknowledgeSerialInterrupt() {
  REG_IF = IRQ_SERIAL;
}

// ---

bool send(u16 data, CancelCallback cancel) {
  while (!didSerialInterruptOccur()) {
    if (cancel())
      return false;
  }
  acknowledgeSerialInterrupt();
  if (assignedPlayerId() != 1)
    return false;
  setData(data);

  return true;
}

bool sendAndExpect(u16 data, u16 expect, CancelCallback cancel) {
  u16 received;
  do {
    if (!send(data, cancel))
      return false;

    received = getDataFromPlayer0();
    bool isReset = received == 0 && expect != 0xFBFB;
    if (isReset)
      return false;
  } while (received != expect);

  return true;
}

u16 sendAndReceiveExcept(u16 data, u16 except, CancelCallback cancel) {
  u16 received;
  do {
    if (!send(data, cancel))
      return DISCONNECTED;

    received = getDataFromPlayer0();
  } while (received == except);

  return received;
}

#endif
