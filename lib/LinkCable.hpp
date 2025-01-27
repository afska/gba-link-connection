#ifndef LINK_CABLE_H
#define LINK_CABLE_H

// --------------------------------------------------------------------------
// A Link Cable connection for Multi-Play mode.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCable* linkCable = new LinkCable();
// - 2) Add the required interrupt service routines: (*)
//       interrupt_init();
//       interrupt_add(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
//       interrupt_add(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
//       interrupt_add(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
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

#include "LinkRawCable.hpp"

#ifndef LINK_CABLE_QUEUE_SIZE
/**
 * @brief Buffer size (how many incoming and outgoing messages the queues can
 * store at max **per player**). The default value is `15`, which seems fine for
 * most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around `390` bytes. There's a double-buffered pending queue (to avoid
 * data races), 1 incoming queue and 1 outgoing queue.
 * \warning You can approximate the usage with `LINK_CABLE_QUEUE_SIZE * 26`.
 */
#define LINK_CABLE_QUEUE_SIZE 15
#endif

LINK_VERSION_TAG LINK_CABLE_VERSION = "vLinkCable/v8.0.0";

#define LINK_CABLE_MAX_PLAYERS LINK_RAW_CABLE_MAX_PLAYERS
#define LINK_CABLE_DEFAULT_TIMEOUT 3
#define LINK_CABLE_DEFAULT_INTERVAL 50
#define LINK_CABLE_DEFAULT_SEND_TIMER_ID 3
#define LINK_CABLE_DISCONNECTED LINK_RAW_CABLE_DISCONNECTED
#define LINK_CABLE_NO_DATA 0x0

/**
 * @brief A Link Cable connection for Multi-Play mode.
 */
class LinkCable {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu8 = Link::vu8;
  using U16Queue = Link::Queue<u16, LINK_CABLE_QUEUE_SIZE>;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
  static constexpr int REMOTE_TIMEOUT_OFFLINE = -1;

 public:
  using BaudRate = LinkRawCable::BaudRate;

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
  explicit LinkCable(BaudRate baudRate = BaudRate::BAUD_RATE_1,
                     u32 timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                     u16 interval = LINK_CABLE_DEFAULT_INTERVAL,
                     u8 sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID) {
    config.baudRate = baudRate;
    config.timeout = timeout;
    config.interval = interval;
    config.sendTimerId = sendTimerId;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   */
  void activate() {
    LINK_READ_TAG(LINK_CABLE_VERSION);

    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    reset();
    clearIncomingMessages();

    LINK_BARRIER;
    isEnabled = true;
    LINK_BARRIER;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

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
   * @brief Returns the number of connected players (`1~4`).
   */
  [[nodiscard]] u8 playerCount() { return state.playerCount; }

  /**
   * @brief Returns the current player ID (`0~3`).
   */
  [[nodiscard]] u8 currentPlayerId() { return state.currentPlayerId; }

  /**
   * @brief Collects available messages from interrupts for later processing
   * with `read(...)`. Call this method whenever you need to fetch new data, and
   * always process all messages before calling it again.
   */
  void sync() {
    if (!isEnabled)
      return;

    LINK_BARRIER;
    isReadingMessages = true;
    LINK_BARRIER;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++)
      move(_state.readyToSyncMessages[i], state.syncedIncomingMessages[i]);

    LINK_BARRIER;
    isReadingMessages = false;
    LINK_BARRIER;

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
   * \warning If `data` is invalid or the send queue is full, a `false` will be
   * returned.
   */
  bool send(u16 data) {
    if (data == LINK_CABLE_DISCONNECTED || data == LINK_CABLE_NO_DATA ||
        _state.outgoingMessages.isFull())
      return false;

    _state.outgoingMessages.syncPush(data);
    return true;
  }

  /**
   * @brief Returns whether the internal receive queue lost messages at some
   * point due to being full. This can happen if your queue size is too low, if
   * you receive too much data without calling `sync(...)` enough times, or if
   * you don't `read(...)` enough messages before the next `sync()` call. After
   * this call, the overflow flag is cleared if `clear` is `true` (default
   * behavior).
   */
  bool didQueueOverflow(bool clear = true) {
    bool overflow = false;

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      overflow = overflow || _state.newMessages[i].overflow ||
                 _state.readyToSyncMessages[i].overflow;
      if (clear) {
        _state.newMessages[i].overflow = false;
        state.syncedIncomingMessages[i].overflow = false;
      }
    }

    return overflow;
  }

  /**
   * @brief Restarts the send timer without disconnecting.
   * \warning Call this if you changed `config.interval`.
   */
  void resetTimer() {
    if (!isEnabled)
      return;

    stopTimer();
    startTimer();
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

    if (!LinkRawCable::allReady() || LinkRawCable::hasError()) {
      reset();
      return;
    }

    auto response = LinkRawCable::getData();
    state.currentPlayerId = response.playerId;

    _state.IRQFlag = true;
    _state.IRQTimeout = 0;

    u8 newPlayerCount = 0;
    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      u16 data = response.data[i];

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

    LinkRawCable::setData(LINK_CABLE_NO_DATA);

    if (!LinkRawCable::isMasterNode())
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

    if (LinkRawCable::isMasterNode() && LinkRawCable::allReady() &&
        !LinkRawCable::isSending())
      sendPendingData();

    copyState();
  }

  struct Config {
    BaudRate baudRate;
    u32 timeout;   // can be changed in realtime
    u16 interval;  // can be changed in realtime, but call `resetTimer()`
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
    vu8 playerCount;
    vu8 currentPlayerId;
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

  LinkRawCable linkRawCable;
  ExternalState state;
  InternalState _state;
  volatile bool isEnabled = false;
  volatile bool isReadingMessages = false;

  bool didTimeout() { return _state.IRQTimeout >= config.timeout; }

  void sendPendingData() {
    if (_state.outgoingMessages.isWriting())
      return;

    LINK_BARRIER;

    transfer(_state.outgoingMessages.pop());
  }

  void transfer(u16 data) {
    LinkRawCable::setData(data);

    if (LinkRawCable::isMasterNode())
      LinkRawCable::startTransfer();
  }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    state.playerCount = 1;
    state.currentPlayerId = 0;

    _state.outgoingMessages.syncClear();

    for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++) {
      if (!isReadingMessages)
        _state.readyToSyncMessages[i].clear();

      _state.newMessages[i].clear();
      setOffline(i);

      _state.newMessages[i].overflow = false;
      state.syncedIncomingMessages[i].overflow = false;
    }
    _state.IRQFlag = false;
    _state.IRQTimeout = 0;
  }

  void stop() {
    stopTimer();
    linkRawCable.deactivate();
  }

  void start() {
    startTimer();
    linkRawCable.activate(config.baudRate);
    LinkRawCable::setInterruptsOn();
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
    while (!src.isEmpty() && !dst.isFull())
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
