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

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#ifndef LINK_CABLE_QUEUE_SIZE
/**
 * @brief Buffer size (how many incoming and outgoing messages the queues can
 * store at max **per player**). The default value is `15`, which seems fine for
 * most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around `390` bytes. There's a double-buffered pending queue (to avoid
 * data races), 1 incoming queue and 1 outgoing queue. \warning You can
 * approximate the usage with `LINK_CABLE_QUEUE_SIZE * 26`.
 */
#define LINK_CABLE_QUEUE_SIZE 15
#endif

static volatile char LINK_CABLE_VERSION[] = "LinkCable/v7.0.0";

#define LINK_CABLE_MAX_PLAYERS 4
#define LINK_CABLE_DEFAULT_TIMEOUT 3
#define LINK_CABLE_DEFAULT_INTERVAL 50
#define LINK_CABLE_DEFAULT_SEND_TIMER_ID 3
#define LINK_CABLE_DISCONNECTED 0xffff
#define LINK_CABLE_NO_DATA 0x0
#define LINK_CABLE_BARRIER asm volatile("" ::: "memory")

/**
 * @brief A Link Cable connection for Multi-Play mode.
 */
class LinkCable {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using vu32 = volatile unsigned int;
  using vs32 = volatile signed int;
  using U16Queue = Link::Queue<u16, LINK_CABLE_QUEUE_SIZE>;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
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

  /**
   * @brief Constructs a new LinkCable object.
   * @param baudRate Sets a specific baud rate.
   * @param timeout Number of *frames* without a `SERIAL` IRQ to reset the
   * connection.
   * @param interval Number of *1024-cycle ticks* (61.04μs) between transfers
   * *(50 = 3.052ms)*. It's the interval of Timer #`sendTimerId`. Lower values
   * will transfer faster but also consume more CPU.
   * @param sendTimerId `(0~3)` GBA Timer to use for sending.
   * \warning You can use `Link::perFrame(...)` to convert from *packets per
   * frame* to *interval values*.
   */
  explicit LinkCable(BaudRate baudRate = BAUD_RATE_1,
                     u32 timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                     u16 interval = LINK_CABLE_DEFAULT_INTERVAL,
                     u8 sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID) {
    this->config.baudRate = baudRate;
    this->config.timeout = timeout;
    this->config.interval = interval;
    this->config.sendTimerId = sendTimerId;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   */
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

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    LINK_CABLE_BARRIER;
    isEnabled = false;
    LINK_CABLE_BARRIER;

    resetState();
    stop();
    clearIncomingMessages();
  }

  /**
   * @brief Returns `true` if there are at least 2 connected players.
   */
  [[nodiscard]] bool isConnected() {
    return state.playerCount > 1 && state.currentPlayerId < state.playerCount;
  }

  /**
   * @brief Returns the number of connected players (`0~4`).
   */
  [[nodiscard]] u8 playerCount() { return state.playerCount; }

  /**
   * @brief Returns the current player ID (`0~3`).
   */
  [[nodiscard]] u8 currentPlayerId() { return state.currentPlayerId; }

  /**
   * @brief Call this method every time you need to fetch new data.
   */
  void sync() {
    if (!isEnabled)
      return;

    LINK_CABLE_BARRIER;
    isReadingMessages = true;
    LINK_CABLE_BARRIER;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++)
      move(_state.readyToSyncMessages[i], state.syncedIncomingMessages[i]);

    LINK_CABLE_BARRIER;
    isReadingMessages = false;
    LINK_CABLE_BARRIER;

    if (!isConnected())
      clearIncomingMessages();
  }

  /**
   * @brief Waits for data from player #`playerId`. Returns `true` on success,
   * or `false` on disconnection.
   * @param playerId A player ID.
   */
  bool waitFor(u8 playerId) {
    return waitFor(playerId, []() { return false; });
  }

  /**
   * @brief Waits for data from player #`playerId`. Returns `true` on success,
   * or `false` on disconnection.
   * @param playerId ID of player to wait data from.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the wait be aborted.
   */
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

  /**
   * @brief Returns `true` if there are pending messages from player
   * #`playerId`.
   * @param playerId A player ID.
   * \warning Keep in mind that if this returns `false`, it will keep doing so
   * until you *fetch new data* with `sync()`.
   */
  [[nodiscard]] bool canRead(u8 playerId) {
    return !state.syncedIncomingMessages[playerId].isEmpty();
  }

  /**
   * @brief Dequeues and returns the next message from player #`playerId`.
   * @param playerId A player ID.
   * \warning If there's no data from that player, a `0` will be returned.
   */
  u16 read(u8 playerId) { return state.syncedIncomingMessages[playerId].pop(); }

  /**
   * @brief Returns the next message from player #`playerId` without dequeuing
   * it.
   * @param playerId A player ID.
   * \warning If there's no data from that player, a `0` will be returned.
   */
  [[nodiscard]] u16 peek(u8 playerId) {
    return state.syncedIncomingMessages[playerId].peek();
  }

  /**
   * @brief Sends `data` to all connected players.
   * @param data The value to be sent.
   */
  void send(u16 data) {
    if (data == LINK_CABLE_DISCONNECTED || data == LINK_CABLE_NO_DATA)
      return;

    _state.outgoingMessages.syncPush(data);
  }

  /**
   * @brief This method is called by the VBLANK interrupt handler.
   * \warning This is internal API!
   */
  void _onVBlank() {
    if (!isEnabled)
      return;

    if (!_state.IRQFlag)
      _state.IRQTimeout++;
    _state.IRQFlag = false;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      if (isOnline(i) && !_state.msgFlags[i])
        _state.msgTimeouts[i]++;
      _state.msgFlags[i] = false;
    }

    if (didTimeout()) {
      reset();
      return;
    }

    copyState();
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
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

      if (data != LINK_CABLE_DISCONNECTED) {
        if (data != LINK_CABLE_NO_DATA && i != state.currentPlayerId)
          _state.newMessages[i].push(data);
        newPlayerCount++;
        setOnline(i);
      } else if (isOnline(i)) {
        if (_state.msgTimeouts[i] >= (int)config.timeout) {
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

    Link::_REG_SIOMLT_SEND = LINK_CABLE_NO_DATA;

    if (!isMaster())
      sendPendingData();

    copyState();
  }

  /**
   * @brief This method is called by the TIMER interrupt handler.
   * \warning This is internal API!
   */
  void _onTimer() {
    if (!isEnabled)
      return;

    if (isMaster() && isReady() && !isSending())
      sendPendingData();

    copyState();
  }

  struct Config {
    BaudRate baudRate;
    u32 timeout;
    u32 interval;
    u8 sendTimerId;
  };

  /**
   * @brief LinkCable configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
  struct ExternalState {
    U16Queue syncedIncomingMessages[LINK_CABLE_MAX_PLAYERS];
    u8 playerCount;
    u8 currentPlayerId;
  };

  struct InternalState {
    U16Queue outgoingMessages;
    U16Queue readyToSyncMessages[LINK_CABLE_MAX_PLAYERS];
    U16Queue newMessages[LINK_CABLE_MAX_PLAYERS];
    u32 IRQTimeout;
    int msgTimeouts[LINK_CABLE_MAX_PLAYERS];
    bool msgFlags[LINK_CABLE_MAX_PLAYERS];
    bool IRQFlag;
  };

  ExternalState state;
  InternalState _state;
  volatile bool isEnabled = false;
  volatile bool isReadingMessages = false;

  bool isMaster() { return !isBitHigh(BIT_SLAVE); }
  bool isReady() { return isBitHigh(BIT_READY); }
  bool hasError() { return isBitHigh(BIT_ERROR); }
  bool isSending() { return isBitHigh(BIT_START); }
  bool didTimeout() { return _state.IRQTimeout >= config.timeout; }

  void sendPendingData() {
    if (_state.outgoingMessages.isWriting())
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

    _state.outgoingMessages.syncClear();

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      if (!isReadingMessages)
        _state.readyToSyncMessages[i].clear();

      _state.newMessages[i].clear();
      setOffline(i);
    }
    _state.IRQFlag = false;
    _state.IRQTimeout = 0;
  }

  void stop() {
    Link::_REG_SIOMLT_SEND = LINK_CABLE_NO_DATA;
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
      state.syncedIncomingMessages[i].clear();
  }

  void copyState() {
    if (isReadingMessages)
      return;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      if (isOnline(i))
        move(_state.newMessages[i], _state.readyToSyncMessages[i]);
      else
        _state.readyToSyncMessages[i].clear();
    }
  }

  void move(U16Queue& src, U16Queue& dst) {
    while (!src.isEmpty())
      dst.push(src.pop());
  }

  bool isOnline(u8 playerId) {
    return _state.msgTimeouts[playerId] != REMOTE_TIMEOUT_OFFLINE;
  }

  void setOnline(u8 playerId) {
    _state.msgTimeouts[playerId] = 0;
    _state.msgFlags[playerId] = true;
  }

  void setOffline(u8 playerId) {
    _state.msgTimeouts[playerId] = REMOTE_TIMEOUT_OFFLINE;
    _state.msgFlags[playerId] = false;
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

/**
 * @brief VBLANK interrupt handler.
 */
inline void LINK_CABLE_ISR_VBLANK() {
  linkCable->_onVBlank();
}

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_CABLE_ISR_SERIAL() {
  linkCable->_onSerial();
}

/**
 * @brief TIMER interrupt handler.
 */
inline void LINK_CABLE_ISR_TIMER() {
  linkCable->_onTimer();
}

/**
 * NOTES:
 * For end users:
 *   - `sync()` fills an incoming queue (`syncedIncomingMessages`).
 *   - `read(...)` pops one message from that queue.
 *   - `send(...)` pushes one message to an outgoing queue (`outgoingMessages`).
 * Behind the curtains:
 *   - On each SERIAL IRQ:
 *     -> Each new message is pushed to `newMessages`.
 *   - On each VBLANK, SERIAL, or TIMER IRQ:
 *     -> **If the user is not syncing**:
 *       -> All `newMessages` are moved to `readyToSyncMessages`.
 *   - If (playerId == 0 && TIMER_IRQ) || (playerId > 0 && SERIAL_IRQ):
 *     -> **If the user is not sending**:
 *       -> Pops one message from `outgoingMessages` and transfers it.
 *   - `sync()` moves all `readyToSyncMessages` to `syncedIncomingMessages`.
 */

#endif  // LINK_CABLE_H
