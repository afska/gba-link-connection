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
// - 4) Sync:
//       linkUniversal->sync();
//       // (put this line at the start of your game loop)
// - 5) Send/read messages by using:
//       bool isConnected = linkCable->isConnected();
//       u8 playerCount = linkCable->playerCount();
//       u8 currentPlayerId = linkCable->currentPlayerId();
//       linkCable->send(0x1234);
//       if (isConnected && linkCable->canRead(!currentPlayerId)) {
//         u16 message = linkCable->read(!currentPlayerId);
//         // ...
//       }
// --------------------------------------------------------------------------
// (*1) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//      That causes packet loss. You REALLY want to use libugba's instead.
//      (see examples)
// --------------------------------------------------------------------------
// (*2) The hardware is very sensitive to timing. Make sure that
//      `LINK_CABLE_ISR_SERIAL()` is handled on time. That means:
//      Be careful with DMA usage (which stops the CPU), and write short
//      interrupt handlers (or activate nested interrupts by setting
//      `REG_IME=1` at the start of your handlers).
// --------------------------------------------------------------------------
// `send(...)` restrictions:
// - 0xFFFF and 0x0 are reserved values, so don't send them!
//   (they mean 'disconnected' and 'no data' respectively)
// --------------------------------------------------------------------------

#include "_link_common.hpp"

// Buffer size
#define LINK_CABLE_QUEUE_SIZE 15

static volatile char LINK_CABLE_VERSION[] = "LinkCable/v7.0.0";

#define LINK_CABLE_MAX_PLAYERS 4
#define LINK_CABLE_DEFAULT_TIMEOUT 3
#define LINK_CABLE_DEFAULT_REMOTE_TIMEOUT 5
#define LINK_CABLE_DEFAULT_INTERVAL 50
#define LINK_CABLE_DEFAULT_SEND_TIMER_ID 3
#define LINK_CABLE_BARRIER asm volatile("" ::: "memory")

class LinkCable {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using vs32 = volatile signed int;
  using vu32 = volatile unsigned int;

  static constexpr u16 DISCONNECTED = 0xffff;
  static constexpr u16 NO_DATA = 0x0;
  static constexpr u16 BASE_FREQUENCY = Link::_TM_FREQ_1024;
  static constexpr int REMOTE_TIMEOUT_OFFLINE = -1;
  static constexpr int BIT_SLAVE = 2;
  static constexpr int BIT_READY = 3;
  static constexpr int BITS_PLAYER_ID = 4;
  static constexpr int BIT_ERROR = 6;
  static constexpr int BIT_START = 7;
  static constexpr int BIT_MULTIPLAYER = 13;
  static constexpr int BIT_IRQ = 14;
  static constexpr int BIT_GENERAL_PURPOSE_LOW = 14;
  static constexpr int BIT_GENERAL_PURPOSE_HIGH = 15;

 public:
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };

  class U16Queue {
   public:
    void push(u16 item) {
      if (isFull())
        pop();

      rear = (rear + 1) % LINK_CABLE_QUEUE_SIZE;
      arr[rear] = item;
      count++;
    }

    u16 pop() {
      if (isEmpty())
        return NO_DATA;

      auto x = arr[front];
      front = (front + 1) % LINK_CABLE_QUEUE_SIZE;
      count--;

      return x;
    }

    u16 peek() {
      if (isEmpty())
        return NO_DATA;

      return arr[front];
    }

    void clear() {
      front = count = 0;
      rear = -1;
    }

    u32 size() { return count; }
    bool isEmpty() { return size() == 0; }
    bool isFull() { return size() == LINK_CABLE_QUEUE_SIZE; }

   private:
    u16 arr[LINK_CABLE_QUEUE_SIZE];
    vs32 front = 0;
    vs32 rear = -1;
    vu32 count = 0;
  };

  explicit LinkCable(BaudRate baudRate = BAUD_RATE_1,
                     u32 timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                     u32 remoteTimeout = LINK_CABLE_DEFAULT_REMOTE_TIMEOUT,
                     u16 interval = LINK_CABLE_DEFAULT_INTERVAL,
                     u8 sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID) {
    this->config.baudRate = baudRate;
    this->config.timeout = timeout;
    this->config.remoteTimeout = remoteTimeout;
    this->config.interval = interval;
    this->config.sendTimerId = sendTimerId;
  }

  bool isActive() { return isEnabled; }

  void activate() {
    LINK_CABLE_BARRIER;
    isEnabled = false;
    LINK_CABLE_BARRIER;

    reset();
    clearIncomingMessages();

    LINK_CABLE_BARRIER;
    isEnabled = true;
    LINK_CABLE_BARRIER;
  }

  void deactivate() {
    LINK_CABLE_BARRIER;
    isEnabled = false;
    LINK_CABLE_BARRIER;

    resetState();
    stop();
    clearIncomingMessages();
  }

  bool isConnected() {
    return state.playerCount > 1 && state.currentPlayerId < state.playerCount;
  }

  u8 playerCount() { return state.playerCount; }
  u8 currentPlayerId() { return state.currentPlayerId; }

  void sync() {
    if (!isEnabled)
      return;

    LINK_CABLE_BARRIER;
    isReadingMessages = true;
    LINK_CABLE_BARRIER;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++)
      move(_state.pendingMessages[i], state.incomingMessages[i]);

    LINK_CABLE_BARRIER;
    isReadingMessages = false;
    LINK_CABLE_BARRIER;

    if (!isConnected())
      clearIncomingMessages();
  }

  bool waitFor(u8 playerId) {
    return waitFor(playerId, []() { return false; });
  }

  template <typename F>
  bool waitFor(u8 playerId, F cancel) {
    sync();

    while (isConnected() && !canRead(playerId) && !cancel()) {
      Link::_IntrWait(
          1, Link::_IRQ_SERIAL | Link::_TIMER_IRQ_IDS[config.sendTimerId]);
      sync();
    }

    return isConnected() && canRead(playerId);
  }

  bool canRead(u8 playerId) {
    return !state.incomingMessages[playerId].isEmpty();
  }

  u16 read(u8 playerId) { return state.incomingMessages[playerId].pop(); }

  u16 peek(u8 playerId) { return state.incomingMessages[playerId].peek(); }

  void send(u16 data) {
    if (data == DISCONNECTED || data == NO_DATA)
      return;

    LINK_CABLE_BARRIER;
    isAddingMessage = true;
    LINK_CABLE_BARRIER;

    _state.outgoingMessages.push(data);

    LINK_CABLE_BARRIER;
    isAddingMessage = false;
    LINK_CABLE_BARRIER;

    if (isAddingWhileResetting) {
      _state.outgoingMessages.clear();
      isAddingWhileResetting = false;
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

  void _onSerial() {
    if (!isEnabled)
      return;

    if (!isReady() || hasError()) {
      reset();
      return;
    }

    _state.IRQFlag = true;
    _state.IRQTimeout = 0;

    u8 newPlayerCount = 0;
    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      u16 data = Link::_REG_SIOMULTI[i];

      if (data != DISCONNECTED) {
        if (data != NO_DATA && i != state.currentPlayerId)
          _state.newMessages[i].push(data);
        newPlayerCount++;
        setOnline(i);
      } else if (isOnline(i)) {
        _state.timeouts[i]++;

        if (_state.timeouts[i] >= (int)config.remoteTimeout) {
          _state.newMessages[i].clear();
          setOffline(i);
        } else {
          newPlayerCount++;
        }
      }
    }

    state.playerCount = newPlayerCount;
    state.currentPlayerId =
        (Link::_REG_SIOCNT & (0b11 << BITS_PLAYER_ID)) >> BITS_PLAYER_ID;

    if (!isMaster())
      sendPendingData();

    copyState();
  }

  void _onTimer() {
    if (!isEnabled)
      return;

    if (didTimeout()) {
      reset();
      return;
    }

    if (isMaster() && isReady() && !isSending())
      sendPendingData();

    copyState();
  }

  struct Config {
    BaudRate baudRate;
    u32 timeout;
    u32 remoteTimeout;
    u32 interval;
    u8 sendTimerId;
  };

  Config config;

 private:
  struct ExternalState {
    U16Queue incomingMessages[LINK_CABLE_MAX_PLAYERS];
    u8 playerCount;
    u8 currentPlayerId;
  };

  struct InternalState {
    U16Queue outgoingMessages;
    U16Queue pendingMessages[LINK_CABLE_MAX_PLAYERS];
    U16Queue newMessages[LINK_CABLE_MAX_PLAYERS];
    int timeouts[LINK_CABLE_MAX_PLAYERS];
    bool IRQFlag;
    u32 IRQTimeout;
  };

  ExternalState state;
  InternalState _state;
  volatile bool isEnabled = false;
  volatile bool isReadingMessages = false;
  volatile bool isAddingMessage = false;
  volatile bool isAddingWhileResetting = false;

  bool isMaster() { return !isBitHigh(BIT_SLAVE); }
  bool isReady() { return isBitHigh(BIT_READY); }
  bool hasError() { return isBitHigh(BIT_ERROR); }
  bool isSending() { return isBitHigh(BIT_START); }
  bool didTimeout() { return _state.IRQTimeout >= config.timeout; }

  void sendPendingData() {
    if (isAddingMessage)
      return;

    LINK_CABLE_BARRIER;

    transfer(_state.outgoingMessages.pop());
  }

  void transfer(u16 data) {
    Link::_REG_SIOMLT_SEND = data;

    if (isMaster())
      setBitHigh(BIT_START);
  }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    state.playerCount = 0;
    state.currentPlayerId = 0;

    if (isAddingMessage || isAddingWhileResetting)
      isAddingWhileResetting = true;
    else
      _state.outgoingMessages.clear();

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      if (!isReadingMessages)
        _state.pendingMessages[i].clear();

      _state.newMessages[i].clear();
      setOffline(i);
    }
    _state.IRQFlag = false;
    _state.IRQTimeout = 0;
  }

  void stop() {
    stopTimer();
    setGeneralPurposeMode();
  }

  void start() {
    startTimer();
    setMultiPlayMode();
    setInterruptsOn();
  }

  void stopTimer() {
    Link::_REG_TM[config.sendTimerId].cnt =
        Link::_REG_TM[config.sendTimerId].cnt & (~Link::_TM_ENABLE);
  }

  void startTimer() {
    Link::_REG_TM[config.sendTimerId].start = -config.interval;
    Link::_REG_TM[config.sendTimerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | BASE_FREQUENCY;
  }

  void clearIncomingMessages() {
    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++)
      state.incomingMessages[i].clear();
  }

  void copyState() {
    if (isReadingMessages)
      return;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      if (isOnline(i))
        move(_state.newMessages[i], _state.pendingMessages[i]);
      else
        _state.pendingMessages[i].clear();
    }
  }

  void move(U16Queue& src, U16Queue& dst) {
    while (!src.isEmpty())
      dst.push(src.pop());
  }

  bool isOnline(u8 playerId) {
    return _state.timeouts[playerId] != REMOTE_TIMEOUT_OFFLINE;
  }
  void setOnline(u8 playerId) { _state.timeouts[playerId] = 0; }
  void setOffline(u8 playerId) {
    _state.timeouts[playerId] = REMOTE_TIMEOUT_OFFLINE;
  }

  void setInterruptsOn() { setBitHigh(BIT_IRQ); }

  void setMultiPlayMode() {
    Link::_REG_RCNT = Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_HIGH);
    Link::_REG_SIOCNT = (1 << BIT_MULTIPLAYER);
    Link::_REG_SIOCNT |= config.baudRate;
    Link::_REG_SIOMLT_SEND = 0;
  }

  void setGeneralPurposeMode() {
    Link::_REG_RCNT = (Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }

  bool isBitHigh(u8 bit) { return (Link::_REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { Link::_REG_SIOCNT |= 1 << bit; }
  void setBitLow(u8 bit) { Link::_REG_SIOCNT &= ~(1 << bit); }
};

extern LinkCable* linkCable;

inline void LINK_CABLE_ISR_VBLANK() {
  linkCable->_onVBlank();
}

inline void LINK_CABLE_ISR_SERIAL() {
  linkCable->_onSerial();
}

inline void LINK_CABLE_ISR_TIMER() {
  linkCable->_onTimer();
}

#endif  // LINK_CABLE_H
