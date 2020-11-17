#ifndef LINK_CONNECTION_H
#define LINK_CONNECTION_H

#include <tonc_bios.h>
#include <tonc_core.h>
#include <tonc_memdef.h>
#include <tonc_memmap.h>

#include <memory>
#include <queue>

#include "utils/SceneUtils.h"  // TODO: REMOVE

#define LINK_MAX_PLAYERS 4
#define LINK_DISCONNECTED 0xFFFF
#define LINK_NO_DATA 0x0
#define LINK_DEFAULT_TIMEOUT 3
#define LINK_DEFAULT_BUFFER_SIZE 10
#define LINK_DEFAULT_TIMER_ID 3
#define LINK_DEFAULT_FREQUENCY 25
#define LINK_BASE_FREQUENCY TM_FREQ_1024
#define LINK_BIT_SLAVE 2
#define LINK_BIT_READY 3
#define LINK_BITS_PLAYER_ID 4
#define LINK_BIT_ERROR 6
#define LINK_BIT_START 7
#define LINK_BIT_MULTIPLAYER 13
#define LINK_BIT_IRQ 14
#define LINK_BIT_GENERAL_PURPOSE_LOW 14
#define LINK_BIT_GENERAL_PURPOSE_HIGH 15
#define LINK_SET_HIGH(REG, BIT) REG |= 1 << BIT
#define LINK_SET_LOW(REG, BIT) REG &= ~(1 << BIT)

// A Link Cable connection for Multi-player mode.

// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkConnection* linkConnection = new LinkConnection();
// - 2) Add the interrupt service routines:
//       irq_add(II_VBLANK, LINK_ISR_VBLANK);
//       irq_add(II_TIMER3, LINK_ISR_TIMER);
//       irq_add(II_SERIAL, LINK_ISR_SERIAL);
// - 3) Send/read messages by using:
//       linkConnection->send(...);
//       linkConnection->linkState

// `data` restrictions:
// 0xFFFF and 0x0 are reserved values, so don't use them
// (they mean 'disconnected' and 'no data' respectively)

void LINK_ISR_VBLANK();
void LINK_ISR_TIMER();
void LINK_ISR_SERIAL();
u16 LINK_QUEUE_POP(std::queue<u16>& q);
void LINK_QUEUE_CLEAR(std::queue<u16>& q);
const u16 LINK_TIMER_IRQ_IDS[] = {IRQ_TIMER0, IRQ_TIMER1, IRQ_TIMER2,
                                  IRQ_TIMER3};

struct LinkState {
  u8 playerCount;
  u8 currentPlayerId;
  std::queue<u16> _incomingMessages[LINK_MAX_PLAYERS];
  std::queue<u16> _outgoingMessages;
  bool _IRQFlag;
  u32 _IRQTimeout;

  bool isConnected() {
    return playerCount > 1 && currentPlayerId < playerCount;
  }

  bool hasMessage(u8 playerId) {
    if (playerId >= playerCount)
      return false;

    return !_incomingMessages[playerId].empty();
  }

  u16 readMessage(u8 playerId) {
    return LINK_QUEUE_POP(_incomingMessages[playerId]);
  }
};

class LinkConnection {
 public:
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  std::unique_ptr<struct LinkState> linkState{new LinkState()};

  explicit LinkConnection(bool startNow = true,
                          BaudRate baudRate = BAUD_RATE_3,
                          u32 timeout = LINK_DEFAULT_TIMEOUT,
                          u32 bufferSize = LINK_DEFAULT_BUFFER_SIZE,
                          u8 timerId = LINK_DEFAULT_TIMER_ID,
                          u32 frequency = LINK_DEFAULT_FREQUENCY) {
    this->baudRate = baudRate;
    this->timeout = timeout;
    this->bufferSize = bufferSize;
    this->timerId = timerId;
    this->frequency = frequency;

    if (startNow)
      activate();
    else
      stop();
  }

  bool isActive() { return isEnabled; }

  void activate() {
    isEnabled = true;
    reset();
  }

  void deactivate() {
    isEnabled = false;
    resetState();
    stop();
  }

  void send(u16 data) {
    if (data == LINK_DISCONNECTED || data == LINK_NO_DATA)
      return;

    push(linkState->_outgoingMessages, data);
  }

  bool isReady() { return isBitHigh(LINK_BIT_READY); }
  bool hasError() { return isBitHigh(LINK_BIT_ERROR); }

  void _onVBlank() {
    if (!isEnabled)
      return;

    if (!linkState->_IRQFlag)
      linkState->_IRQTimeout++;

    linkState->_IRQFlag = false;
  }

  void _onTimer() {
    if (!isEnabled || !isReady())
      return;

    if (linkState->_IRQTimeout > timeout) {
      reset();
      return;
    }

    stopTimer();
    sendPendingData();
    startTimer();
  }

  void _onSerial() {
    if (!isEnabled)
      return;

    waitCycles(1000);  // TODO: PARAMETERIZE
    if (resetIfNeeded())
      return;

    linkState->_IRQFlag = true;
    linkState->_IRQTimeout = 0;

    u8 newPlayerCount = 0;
    for (u32 i = 0; i < LINK_MAX_PLAYERS; i++) {
      u16 data = REG_SIOMULTI[i];

      if (data != LINK_DISCONNECTED) {
        if (data != LINK_NO_DATA)
          push(linkState->_incomingMessages[i], data);
        newPlayerCount++;
      } else
        LINK_QUEUE_CLEAR(linkState->_incomingMessages[i]);
    }

    if (linkState->playerCount == 2 && newPlayerCount == 1) {
      DEBULOG("hey, rompió: " + asStr(REG_SIOMULTI[0]) + "-" +
              asStr(REG_SIOMULTI[1]));
    }
    linkState->playerCount = newPlayerCount;
    linkState->currentPlayerId =
        (REG_SIOCNT & (0b11 << LINK_BITS_PLAYER_ID)) >> LINK_BITS_PLAYER_ID;
  }

 private:
  BaudRate baudRate;
  u32 timeout;
  u32 bufferSize;
  u8 timerId;
  u32 frequency;
  bool isEnabled = false;

  void sendPendingData() {
    transfer(LINK_QUEUE_POP(linkState->_outgoingMessages));
  }

  void transfer(u16 data) {
    REG_SIOMLT_SEND = data;

    waitCycles(1000);  // PARAMETERIZE

    if (isMaster())
      setBitHigh(LINK_BIT_START);
  }

  bool resetIfNeeded() {
    if (hasError()) {
      DEBULOG("resetting: " + asStr(isReady()) + "-" + asStr(hasError()) + "-" +
              asStr(linkState->_IRQTimeout));
      reset();
      return true;
    }

    return false;
  }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    linkState->playerCount = 0;
    linkState->currentPlayerId = 0;
    for (u32 i = 0; i < LINK_MAX_PLAYERS; i++)
      LINK_QUEUE_CLEAR(linkState->_incomingMessages[i]);
    LINK_QUEUE_CLEAR(linkState->_outgoingMessages);
    linkState->_IRQFlag = false;
    linkState->_IRQTimeout = 0;
  }

  void stop() {
    stopTimer();

    LINK_SET_LOW(REG_RCNT, LINK_BIT_GENERAL_PURPOSE_LOW);
    LINK_SET_HIGH(REG_RCNT, LINK_BIT_GENERAL_PURPOSE_HIGH);
  }

  void start() {
    startTimer();

    LINK_SET_LOW(REG_RCNT, LINK_BIT_GENERAL_PURPOSE_HIGH);
    REG_SIOCNT = baudRate;
    REG_SIOMLT_SEND = 0;
    setBitHigh(LINK_BIT_MULTIPLAYER);
    setBitHigh(LINK_BIT_IRQ);
  }

  void stopTimer() { REG_TM[timerId].cnt = REG_TM[timerId].cnt & (~TM_ENABLE); }

  void startTimer() {
    REG_TM[timerId].start = -frequency;
    REG_TM[timerId].cnt = TM_ENABLE | TM_IRQ | LINK_BASE_FREQUENCY;
  }

  void push(std::queue<u16>& q, u16 value) {
    if (q.size() >= bufferSize)
      LINK_QUEUE_POP(q);

    q.push(value);
  }

  void waitCycles(u16 amount) {
    // TODO: Parameterize
    REG_TM[2].start = -amount;
    REG_TM[2].cnt = TM_ENABLE | TM_IRQ | TM_FREQ_1;
    IntrWait(1, LINK_TIMER_IRQ_IDS[2]);
    REG_TM[2].cnt = 0;
  }

  bool isMaster() { return !isBitHigh(LINK_BIT_SLAVE); }
  bool isBitHigh(u8 bit) { return (REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { LINK_SET_HIGH(REG_SIOCNT, bit); }
  void setBitLow(u8 bit) { LINK_SET_LOW(REG_SIOCNT, bit); }
};

extern LinkConnection* linkConnection;

inline void LINK_ISR_VBLANK() {
  linkConnection->_onVBlank();
}

inline void LINK_ISR_TIMER() {
  linkConnection->_onTimer();
}

inline void LINK_ISR_SERIAL() {
  linkConnection->_onSerial();
}

inline u16 LINK_QUEUE_POP(std::queue<u16>& q) {
  if (q.empty())
    return LINK_NO_DATA;

  u16 value = q.front();
  q.pop();
  return value;
}

inline void LINK_QUEUE_CLEAR(std::queue<u16>& q) {
  while (!q.empty())
    LINK_QUEUE_POP(q);
}

#endif  // LINK_CONNECTION_H
