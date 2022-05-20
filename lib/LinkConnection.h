#ifndef LINK_CONNECTION_H
#define LINK_CONNECTION_H

#include <tonc_core.h>
#include <queue>

#define LINK_MAX_PLAYERS 4
#define LINK_DISCONNECTED 0xFFFF
#define LINK_NO_DATA 0x0
#define LINK_DEFAULT_TIMEOUT 3
#define LINK_DEFAULT_REMOTE_TIMEOUT 5
#define LINK_DEFAULT_BUFFER_SIZE 30
#define LINK_DEFAULT_INTERVAL 50
#define LINK_DEFAULT_SEND_TIMER_ID 3
#define LINK_BASE_FREQUENCY TM_FREQ_1024
#define LINK_REMOTE_TIMEOUT_OFFLINE -1
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

// --------------------------------------------------------------------------
// A Link Cable connection for Multi-player mode.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkConnection* linkConnection = new LinkConnection();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_ISR_TIMER);
// - 3) Initialize the library with:
//       linkConnection->activate();
// - 4) Send/read messages by using:
//       u8 playerCount = linkConnection->playerCount();
//       u8 currentPlayerId = linkConnection->currentPlayerId();
//       linkConnection->send(123);
//       if (linkConnection->canRead(!currentPlayerId)
//         linkConnection->read(!currentPlayerId);
// - 5) Mark the current state copy (front buffer) as consumed:
//       linkConnection->consume();
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That can cause packet loss. You might want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// `data` restrictions:
// 0xFFFF and 0x0 are reserved values, so don't use them
// (they mean 'disconnected' and 'no data' respectively)
// --------------------------------------------------------------------------

void LINK_ISR_VBLANK();
void LINK_ISR_TIMER();
void LINK_ISR_SERIAL();
u16 LINK_QUEUE_POP(std::queue<u16>& q);
void LINK_QUEUE_CLEAR(std::queue<u16>& q);
const u16 LINK_TIMER_IRQ_IDS[] = {IRQ_TIMER0, IRQ_TIMER1, IRQ_TIMER2,
                                  IRQ_TIMER3};

struct LinkPublicState {
  std::queue<u16> incomingMessages[LINK_MAX_PLAYERS];
  u8 playerCount;
  u8 currentPlayerId;
  bool isReady = false;
  bool isConsumed = false;
};

struct LinkInternalState {
  std::queue<u16> outgoingMessages;
  int timeouts[LINK_MAX_PLAYERS];
  bool IRQFlag;
  u32 IRQTimeout;
  bool isAddingMessage = false;
};

class LinkConnection {
 public:
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  explicit LinkConnection(BaudRate baudRate = BAUD_RATE_1,
                          u32 timeout = LINK_DEFAULT_TIMEOUT,
                          u32 remoteTimeout = LINK_DEFAULT_REMOTE_TIMEOUT,
                          u32 bufferSize = LINK_DEFAULT_BUFFER_SIZE,
                          u16 interval = LINK_DEFAULT_INTERVAL,
                          u8 sendTimerId = LINK_DEFAULT_SEND_TIMER_ID) {
    this->baudRate = baudRate;
    this->timeout = timeout;
    this->remoteTimeout = remoteTimeout;
    this->bufferSize = bufferSize;
    this->interval = interval;
    this->sendTimerId = sendTimerId;

    stop();
  }

  bool isActive() { return isEnabled; }

  void activate() {
    reset();
    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;
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
    if (!$state.isReady)
      return false;

    return !$state.incomingMessages[playerId].empty();
  }

  u16 read(u8 playerId) {
    if (!$state.isReady)
      return LINK_NO_DATA;

    return LINK_QUEUE_POP($state.incomingMessages[playerId]);
  }

  void consume() { $state.isConsumed = true; }

  void send(u16 data) {
    if (data == LINK_DISCONNECTED || data == LINK_NO_DATA)
      return;

    _state.isAddingMessage = true;
    push(_state.outgoingMessages, data);
    _state.isAddingMessage = false;
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
    for (u32 i = 0; i < LINK_MAX_PLAYERS; i++) {
      u16 data = REG_SIOMULTI[i];

      if (data != LINK_DISCONNECTED) {
        if (data != LINK_NO_DATA && i != state.currentPlayerId)
          push(state.incomingMessages[i], data);
        newPlayerCount++;
        _state.timeouts[i] = 0;
      } else if (_state.timeouts[i] > LINK_REMOTE_TIMEOUT_OFFLINE) {
        _state.timeouts[i]++;

        if (_state.timeouts[i] >= (int)remoteTimeout) {
          LINK_QUEUE_CLEAR(state.incomingMessages[i]);
          _state.timeouts[i] = LINK_REMOTE_TIMEOUT_OFFLINE;
        } else
          newPlayerCount++;
      }
    }

    state.playerCount = newPlayerCount;
    state.currentPlayerId =
        (REG_SIOCNT & (0b11 << LINK_BITS_PLAYER_ID)) >> LINK_BITS_PLAYER_ID;

    if (!isMaster())
      sendPendingData();

    copyState();
  }

 private:
  LinkPublicState state;     // (updated state / back buffer)
  LinkPublicState $state;    // (visible state / front buffer)
  LinkInternalState _state;  // (internal state)
  BaudRate baudRate;
  u32 timeout;
  u32 remoteTimeout;
  u32 bufferSize;
  u32 interval;
  u8 sendTimerId;
  bool isEnabled = false;

  bool isReady() { return isBitHigh(LINK_BIT_READY); }
  bool hasError() { return isBitHigh(LINK_BIT_ERROR); }
  bool isMaster() { return !isBitHigh(LINK_BIT_SLAVE); }
  bool isSending() { return isBitHigh(LINK_BIT_START); }
  bool didTimeout() { return _state.IRQTimeout >= timeout; }

  void sendPendingData() {
    if (_state.isAddingMessage)
      return;

    transfer(LINK_QUEUE_POP(_state.outgoingMessages));
  }

  void transfer(u16 data) {
    REG_SIOMLT_SEND = data;

    if (isMaster())
      setBitHigh(LINK_BIT_START);
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
    for (u32 i = 0; i < LINK_MAX_PLAYERS; i++) {
      LINK_QUEUE_CLEAR(state.incomingMessages[i]);
      _state.timeouts[i] = LINK_REMOTE_TIMEOUT_OFFLINE;
    }
    LINK_QUEUE_CLEAR(_state.outgoingMessages);
    _state.IRQFlag = false;
    _state.IRQTimeout = 0;
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

  void stopTimer() {
    REG_TM[sendTimerId].cnt = REG_TM[sendTimerId].cnt & (~TM_ENABLE);
  }

  void startTimer() {
    REG_TM[sendTimerId].start = -interval;
    REG_TM[sendTimerId].cnt = TM_ENABLE | TM_IRQ | LINK_BASE_FREQUENCY;
  }

  void copyState() {
    if ($state.isReady && !$state.isConsumed)
      return;

    state.isReady = true;
    state.isConsumed = false;
    $state = state;

    for (u32 i = 0; i < LINK_MAX_PLAYERS; i++)
      LINK_QUEUE_CLEAR(state.incomingMessages[i]);
  }

  void push(std::queue<u16>& q, u16 value) {
    if (q.size() >= bufferSize)
      LINK_QUEUE_POP(q);

    q.push(value);
  }

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
