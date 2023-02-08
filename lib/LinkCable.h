#ifndef LINK_CABLE_H
#define LINK_CABLE_H

// --------------------------------------------------------------------------
// A Link Cable connection for Multi-Play mode.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCable* linkCable = new LinkCable();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_CABLE_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_CABLE_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_CABLE_ISR_TIMER);
// - 3) Initialize the library with:
//       linkCable->activate();
// - 4) Send/read messages by using:
//       bool isConnected = linkCable->isConnected();
//       u8 playerCount = linkCable->playerCount();
//       u8 currentPlayerId = linkCable->currentPlayerId();
//       linkCable->send(0x1234);
//       if (isConnected && linkCable->canRead(!currentPlayerId)) {
//         u16 message = linkCable->read(!currentPlayerId);
//         // ...
//       }
// - 5) Mark the current state copy (front buffer) as consumed:
//       linkCable->consume();
//       // (put this line at the end of your game loop)
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That can cause packet loss. You might want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// `data` restrictions:
// - 0xFFFF and 0x0 are reserved values, so don't use them!
//   (they mean 'disconnected' and 'no data' respectively)
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <queue>

#define LINK_CABLE_MAX_PLAYERS 4
#define LINK_CABLE_DISCONNECTED 0xFFFF
#define LINK_CABLE_NO_DATA 0x0
#define LINK_CABLE_DEFAULT_TIMEOUT 3
#define LINK_CABLE_DEFAULT_REMOTE_TIMEOUT 5
#define LINK_CABLE_DEFAULT_BUFFER_SIZE 30
#define LINK_CABLE_DEFAULT_INTERVAL 50
#define LINK_CABLE_DEFAULT_SEND_TIMER_ID 3
#define LINK_CABLE_BASE_FREQUENCY TM_FREQ_1024
#define LINK_CABLE_REMOTE_TIMEOUT_OFFLINE -1
#define LINK_CABLE_BIT_SLAVE 2
#define LINK_CABLE_BIT_READY 3
#define LINK_CABLE_BITS_PLAYER_ID 4
#define LINK_CABLE_BIT_ERROR 6
#define LINK_CABLE_BIT_START 7
#define LINK_CABLE_BIT_MULTIPLAYER 13
#define LINK_CABLE_BIT_IRQ 14
#define LINK_CABLE_BIT_GENERAL_PURPOSE_LOW 14
#define LINK_CABLE_BIT_GENERAL_PURPOSE_HIGH 15
#define LINK_CABLE_SET_HIGH(REG, BIT) REG |= 1 << BIT
#define LINK_CABLE_SET_LOW(REG, BIT) REG &= ~(1 << BIT)
#define LINK_CABLE_BARRIER asm volatile("" ::: "memory")

static volatile char LINK_CABLE_VERSION[] = "LinkCable/v4.3.0";

void LINK_CABLE_ISR_VBLANK();
void LINK_CABLE_ISR_TIMER();
void LINK_CABLE_ISR_SERIAL();
u16 LINK_CABLE_QUEUE_POP(std::queue<u16>& q);
void LINK_CABLE_QUEUE_CLEAR(std::queue<u16>& q);
const u16 LINK_CABLE_TIMER_IRQ_IDS[] = {IRQ_TIMER0, IRQ_TIMER1, IRQ_TIMER2,
                                        IRQ_TIMER3};

class LinkCable {
 public:
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };

  explicit LinkCable(BaudRate baudRate = BAUD_RATE_1,
                     u32 timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                     u32 remoteTimeout = LINK_CABLE_DEFAULT_REMOTE_TIMEOUT,
                     u32 bufferSize = LINK_CABLE_DEFAULT_BUFFER_SIZE,
                     u16 interval = LINK_CABLE_DEFAULT_INTERVAL,
                     u8 sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID) {
    this->config.baudRate = baudRate;
    this->config.timeout = timeout;
    this->config.remoteTimeout = remoteTimeout;
    this->config.bufferSize = bufferSize;
    this->config.interval = interval;
    this->config.sendTimerId = sendTimerId;
  }

  bool isActive() { return isEnabled; }

  void activate() {
    reset();
    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;
    isStateReady = false;
    isStateConsumed = false;
    isResetting = false;
    resetState();
    stop();
  }

  bool isConnected() {
    return $state.playerCount > 1 &&
           $state.currentPlayerId < $state.playerCount;
  }

  u8 playerCount() { return $state.playerCount; }
  u8 currentPlayerId() { return $state.currentPlayerId; }

  bool canRead(u8 playerId) {
    if (!isStateReady || isStateConsumed)
      return false;

    LINK_CABLE_BARRIER;

    return !$state.incomingMessages[playerId].empty();
  }

  u16 read(u8 playerId) {
    if (!isStateReady || isStateConsumed)
      return LINK_CABLE_NO_DATA;

    LINK_CABLE_BARRIER;

    return LINK_CABLE_QUEUE_POP($state.incomingMessages[playerId]);
  }

  void consume() { isStateConsumed = true; }

  void send(u16 data) {
    if (data == LINK_CABLE_DISCONNECTED || data == LINK_CABLE_NO_DATA)
      return;

    LINK_CABLE_BARRIER;
    isAddingMessage = true;
    LINK_CABLE_BARRIER;

    push(_state.outgoingMessages, data);

    LINK_CABLE_BARRIER;
    isAddingMessage = false;
    LINK_CABLE_BARRIER;

    if (isResetting) {
      LINK_CABLE_QUEUE_CLEAR(_state.outgoingMessages);
      isResetting = false;
    }
  }

  void _onVBlank() {
    if (!isEnabled)
      return;

    if (!_state.IRQFlag)
      _state.IRQTimeout++;

    _state.IRQFlag = false;

    copyState();
  }

  void _onTimer() {
    if (!isEnabled)
      return;

    if (didTimeout()) {
      reset();
      copyState();
      return;
    }

    if (isMaster() && isReady() && !isSending())
      sendPendingData();

    copyState();
  }

  void _onSerial() {
    if (!isEnabled)
      return;

    if (resetIfNeeded()) {
      copyState();
      return;
    }

    _state.IRQFlag = true;
    _state.IRQTimeout = 0;

    u8 newPlayerCount = 0;
    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      u16 data = REG_SIOMULTI[i];

      if (data != LINK_CABLE_DISCONNECTED) {
        if (data != LINK_CABLE_NO_DATA && i != state.currentPlayerId)
          push(state.incomingMessages[i], data);
        newPlayerCount++;
        _state.timeouts[i] = 0;
      } else if (_state.timeouts[i] > LINK_CABLE_REMOTE_TIMEOUT_OFFLINE) {
        _state.timeouts[i]++;

        if (_state.timeouts[i] >= (int)config.remoteTimeout) {
          LINK_CABLE_QUEUE_CLEAR(state.incomingMessages[i]);
          _state.timeouts[i] = LINK_CABLE_REMOTE_TIMEOUT_OFFLINE;
        } else
          newPlayerCount++;
      }
    }

    state.playerCount = newPlayerCount;
    state.currentPlayerId =
        (REG_SIOCNT & (0b11 << LINK_CABLE_BITS_PLAYER_ID)) >>
        LINK_CABLE_BITS_PLAYER_ID;

    if (!isMaster())
      sendPendingData();

    copyState();
  }

 private:
  struct PublicState {
    std::queue<u16> incomingMessages[LINK_CABLE_MAX_PLAYERS];
    u8 playerCount;
    u8 currentPlayerId;
  };

  struct InternalState {
    std::queue<u16> outgoingMessages;
    int timeouts[LINK_CABLE_MAX_PLAYERS];
    bool IRQFlag;
    u32 IRQTimeout;
  };

  struct Config {
    BaudRate baudRate;
    u32 timeout;
    u32 remoteTimeout;
    u32 bufferSize;
    u32 interval;
    u8 sendTimerId;
  };

  PublicState state;     // (updated state / back buffer)
  PublicState $state;    // (visible state / front buffer)
  InternalState _state;  // (internal state)
  Config config;
  bool isEnabled = false;
  bool isStateReady = false;
  bool isStateConsumed = false;
  bool isAddingMessage = false;
  bool isResetting = false;

  bool isReady() { return isBitHigh(LINK_CABLE_BIT_READY); }
  bool hasError() { return isBitHigh(LINK_CABLE_BIT_ERROR); }
  bool isMaster() { return !isBitHigh(LINK_CABLE_BIT_SLAVE); }
  bool isSending() { return isBitHigh(LINK_CABLE_BIT_START); }
  bool didTimeout() { return _state.IRQTimeout >= config.timeout; }

  void sendPendingData() {
    if (isAddingMessage)
      return;

    LINK_CABLE_BARRIER;

    transfer(LINK_CABLE_QUEUE_POP(_state.outgoingMessages));
  }

  void transfer(u16 data) {
    REG_SIOMLT_SEND = data;

    if (isMaster())
      setBitHigh(LINK_CABLE_BIT_START);
  }

  bool resetIfNeeded() {
    if (!isReady() || hasError()) {
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
    state.playerCount = 0;
    state.currentPlayerId = 0;
    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      LINK_CABLE_QUEUE_CLEAR(state.incomingMessages[i]);
      _state.timeouts[i] = LINK_CABLE_REMOTE_TIMEOUT_OFFLINE;
    }
    _state.IRQFlag = false;
    _state.IRQTimeout = 0;

    if (isAddingMessage || isResetting)
      isResetting = true;
    else
      LINK_CABLE_QUEUE_CLEAR(_state.outgoingMessages);
  }

  void stop() {
    stopTimer();

    LINK_CABLE_SET_LOW(REG_RCNT, LINK_CABLE_BIT_GENERAL_PURPOSE_LOW);
    LINK_CABLE_SET_HIGH(REG_RCNT, LINK_CABLE_BIT_GENERAL_PURPOSE_HIGH);
  }

  void start() {
    startTimer();

    LINK_CABLE_SET_LOW(REG_RCNT, LINK_CABLE_BIT_GENERAL_PURPOSE_HIGH);
    REG_SIOCNT = config.baudRate;
    REG_SIOMLT_SEND = 0;
    setBitHigh(LINK_CABLE_BIT_MULTIPLAYER);
    setBitHigh(LINK_CABLE_BIT_IRQ);
  }

  void stopTimer() {
    REG_TM[config.sendTimerId].cnt =
        REG_TM[config.sendTimerId].cnt & (~TM_ENABLE);
  }

  void startTimer() {
    REG_TM[config.sendTimerId].start = -config.interval;
    REG_TM[config.sendTimerId].cnt =
        TM_ENABLE | TM_IRQ | LINK_CABLE_BASE_FREQUENCY;
  }

  void copyState() {
    if (isStateReady && !isStateConsumed)
      return;

    LINK_CABLE_BARRIER;
    $state.playerCount = state.playerCount;
    $state.currentPlayerId = state.currentPlayerId;
    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      $state.incomingMessages[i].swap(state.incomingMessages[i]);
      LINK_CABLE_QUEUE_CLEAR(state.incomingMessages[i]);
    }
    LINK_CABLE_BARRIER;
    isStateReady = true;
    isStateConsumed = false;
    LINK_CABLE_BARRIER;
  }

  void push(std::queue<u16>& q, u16 value) {
    if (q.size() >= config.bufferSize)
      LINK_CABLE_QUEUE_POP(q);

    q.push(value);
  }

  bool isBitHigh(u8 bit) { return (REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { LINK_CABLE_SET_HIGH(REG_SIOCNT, bit); }
  void setBitLow(u8 bit) { LINK_CABLE_SET_LOW(REG_SIOCNT, bit); }
};

extern LinkCable* linkCable;

inline void LINK_CABLE_ISR_VBLANK() {
  linkCable->_onVBlank();
}

inline void LINK_CABLE_ISR_TIMER() {
  linkCable->_onTimer();
}

inline void LINK_CABLE_ISR_SERIAL() {
  linkCable->_onSerial();
}

inline u16 LINK_CABLE_QUEUE_POP(std::queue<u16>& q) {
  if (q.empty())
    return LINK_CABLE_NO_DATA;

  u16 value = q.front();
  q.pop();
  return value;
}

inline void LINK_CABLE_QUEUE_CLEAR(std::queue<u16>& q) {
  while (!q.empty())
    LINK_CABLE_QUEUE_POP(q);
}

#endif  // LINK_CABLE_H
