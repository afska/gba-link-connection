#ifndef LINK_WIRELESS_H
#define LINK_WIRELESS_H

// --------------------------------------------------------------------------
// A high level driver for the GBA Wireless Adapter.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWireless* linkWireless = new LinkWireless();
// - 2) Add the required interrupt service routines: (*)
//       interrupt_init();
//       interrupt_add(INTR_VBLANK, LINK_WIRELESS_ISR_VBLANK);
//       interrupt_add(INTR_SERIAL, LINK_WIRELESS_ISR_SERIAL);
//       interrupt_add(INTR_TIMER3, LINK_WIRELESS_ISR_TIMER);
// - 3) Initialize the library with:
//       linkWireless->activate();
// - 4) Start a server:
//       linkWireless->serve();
//
//       // `getState()` should return SERVING now...
//       // `currentPlayerId()` should return 0
//       // `playerCount()` should return the number of active consoles
// - 5) Connect to a server:
//       LinkWireless::Server servers[LINK_WIRELESS_MAX_SERVERS];
//       u32 serverCount;
//       linkWireless->getServers(servers, serverCount);
//       if (serverCount == 0) return;
//
//       linkWireless->connect(servers[0].id);
//       while (linkWireless->getState() == LinkWireless::State::CONNECTING)
//         linkWireless->keepConnecting();
//
//       // `getState()` should return CONNECTED now...
//       // `currentPlayerId()` should return 1, 2, 3, or 4 (the host is 0)
//       // `playerCount()` should return the number of active consoles
// - 6) Send data:
//       linkWireless->send(0x1234);
// - 7) Receive data:
//       LinkWireless::Message messages[LINK_WIRELESS_QUEUE_SIZE];
//       u32 receivedCount;
//       linkWireless->receive(messages, receivedCount);
// - 8) Disconnect:
//       linkWireless->activate();
//       // (resets the adapter)
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include "LinkRawWireless.hpp"

#ifndef LINK_WIRELESS_QUEUE_SIZE
/**
 * @brief Buffer size (how many incoming and outgoing messages the queues can
 * store at max **per player**). The default value is `30`, which seems fine for
 * most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around `480` bytes. There's a double-buffered incoming queue and a
 * double-buffered outgoing queue (to avoid data races).
 * \warning You can approximate the usage with `LINK_WIRELESS_QUEUE_SIZE * 16`.
 */
#define LINK_WIRELESS_QUEUE_SIZE 30
#endif

#ifndef LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
/**
 * @brief Max server transfer length per timer tick. Must be in the range
 * `[6;21]`. The default value is `11`. Higher values will use the bandwidth
 * more efficiently but also consume more CPU!
 * \warning This is measured in words (1 message = 1 halfword). One word is used
 * as a header, so a max transfer length of 11 could transfer up to 20 messages.
 */
#define LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH 11
#endif

#ifndef LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH
/**
 * @brief Max client transfer length per timer tick. Must be in the range
 * `[2;4]`. The default value is `4`. Changing this is not recommended, it's
 * already too low.
 * \warning This is measured in words (1 message = 1 halfword). One halfword is
 * used as a header, so a max transfer length of 4 could transfer up to 7
 * messages.
 */
#define LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#endif

#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM
/**
 * @brief Put Interrupt Service Routines (ISR) in IWRAM (uncomment to enable).
 * This can significantly improve performance due to its faster access, but it's
 * disabled by default to conserve IWRAM space, which is limited.
 * \warning If you enable this, make sure that `lib/iwram_code/LinkWireless.cpp`
 * gets compiled! For example, in a Makefile-based project, verify that the
 * directory is in your `SRCDIRS` list.
 */
// #define LINK_WIRELESS_PUT_ISR_IN_IWRAM
#endif

#ifndef LINK_WIRELESS_ENABLE_NESTED_IRQ
/**
 * @brief Allow LINK_WIRELESS_ISR_* functions to be interrupted (uncomment to
 * enable).
 * This can be useful, for example, if your audio engine requires calling a
 * VBlank handler with precise timing.
 */
// #define LINK_WIRELESS_ENABLE_NESTED_IRQ
#endif

// --- LINK_WIRELESS_PUT_ISR_IN_IWRAM knobs ---
#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL
#define LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL 1
#endif
#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER
#define LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER 1
#endif
#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL_LEVEL
#define LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL_LEVEL "-Ofast"
#endif
#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER_LEVEL
#define LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER_LEVEL "-Ofast"
#endif
//---

LINK_VERSION_TAG LINK_WIRELESS_VERSION = "vLinkWireless/v8.0.2";

#define LINK_WIRELESS_MAX_PLAYERS LINK_RAW_WIRELESS_MAX_PLAYERS
#define LINK_WIRELESS_MIN_PLAYERS 2
#define LINK_WIRELESS_MAX_SERVERS LINK_RAW_WIRELESS_MAX_SERVERS
#define LINK_WIRELESS_MAX_GAME_ID 0x7FFF
#define LINK_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_WIRELESS_DEFAULT_TIMEOUT 10
#define LINK_WIRELESS_DEFAULT_INTERVAL 75
#define LINK_WIRELESS_DEFAULT_SEND_TIMER_ID 3

#define LINK_WIRELESS_RESET_IF_NEEDED                   \
  if (!isEnabled)                                       \
    return false;                                       \
  if (linkRawWireless.getState() == State::NEEDS_RESET) \
    if (!reset())                                       \
      return false;

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#if LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL == 1
#define LINK_WIRELESS_SERIAL_ISR LINK_INLINE
#else
#define LINK_WIRELESS_SERIAL_ISR
#endif
#if LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER == 1
#define LINK_WIRELESS_TIMER_ISR LINK_INLINE
#else
#define LINK_WIRELESS_TIMER_ISR
#endif

#define LINK_WIRELESS_ISR_FUNC(name, params, args, body) \
  void name params;                                      \
  LINK_INLINE void _##name params body
#else
#define LINK_WIRELESS_SERIAL_ISR
#define LINK_WIRELESS_TIMER_ISR
#define LINK_WIRELESS_ISR_FUNC(name, params, args, body) \
  void name params {                                     \
    _##name args;                                        \
  }                                                      \
  LINK_INLINE void _##name params body
#endif

/**
 * @brief A high level driver for the GBA Wireless Adapter.
 */
class LinkWireless {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu8 = Link::vu8;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
  static constexpr int BROADCAST_SEARCH_WAIT_FRAMES = 60;
  static constexpr int MAX_PACKET_IDS_SERVER = 1 << 6;
  static constexpr int MAX_PACKET_IDS_CLIENT = 1 << 4;
  static constexpr int MAX_INFLIGHT_PACKETS_SERVER =
      MAX_PACKET_IDS_SERVER / 2 - 1;
  static constexpr int MAX_INFLIGHT_PACKETS_CLIENT =
      MAX_PACKET_IDS_CLIENT / 2 - 1;
  static constexpr int NO_ID_ASSIGNED_YET = 0xFF;
  static constexpr u32 NO_ACK_RECEIVED_YET = 0xFFFFFFFF;
  static constexpr int HAS_FIRST_MSG_MASK = 0b10000;
  static constexpr int MAX_PLAYER_BITMAP_ENTRIES = 5;
  static constexpr int PLAYER_ID_BITS = 3;
  static constexpr int PLAYER_ID_MASK = 0b111;
  static constexpr int BIT_HAS_MORE = 15;

 public:
// #define LINK_WIRELESS_PROFILING_ENABLED
#ifdef LINK_WIRELESS_PROFILING_ENABLED
  u32 vblankTime = 0;
  u32 serialTime = 0;
  u32 timerTime = 0;
  u32 vblankIRQs = 0;
  u32 serialIRQs = 0;
  u32 timerIRQs = 0;
#endif

  using State = LinkRawWireless::State;
  using SignalLevelResponse = LinkRawWireless::SignalLevelResponse;

  enum class Error {
    // User errors
    NONE = 0,
    WRONG_STATE = 1,
    GAME_NAME_TOO_LONG = 2,
    USER_NAME_TOO_LONG = 3,
    BUFFER_IS_FULL = 4,
    // Communication errors
    COMMAND_FAILED = 5,
    CONNECTION_FAILED = 6,
    SEND_DATA_FAILED = 7,
    RECEIVE_DATA_FAILED = 8,
    ACKNOWLEDGE_FAILED = 9,
    TIMEOUT = 10,
    REMOTE_TIMEOUT = 11,
    BUSY_TRY_AGAIN = 12,
  };

  struct Message {
    u16 data = 0;
    u8 playerId = 0;
    u8 packetId = NO_ID_ASSIGNED_YET;
  };

  struct Server {
    u16 id = 0;
    u16 gameId;
    char gameName[LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char userName[LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    u8 currentPlayerCount;

    bool isFull() { return currentPlayerCount == 0; }
  };

  /**
   * @brief Constructs a new LinkWireless object.
   * @param forwarding If `true`, the server forwards all messages to the
   * clients. Otherwise, clients only see messages sent from the server
   * (ignoring other peers).
   * @param retransmission If `true`, the library handles retransmission for
   * you, so there should be no packet loss.
   * @param maxPlayers `(2~5)` Maximum number of allowed players.
   * @param timeout Number of *frames* without receiving *any* data to reset the
   * connection.
   * @param interval Number of *1024-cycle ticks* (61.04μs) between transfers
   * *(75 = 4.578ms)*. It's the interval of Timer #`sendTimerId`. Lower values
   * will transfer faster but also consume more CPU.
   * @param sendTimerId `(0~3)` GBA Timer to use for sending.
   * \warning You can use `Link::perFrame(...)` to convert from *packets per
   * frame* to *interval values*.
   */
  explicit LinkWireless(bool forwarding = true,
                        bool retransmission = true,
                        u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS,
                        u32 timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
                        u16 interval = LINK_WIRELESS_DEFAULT_INTERVAL,
                        u8 sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID) {
    config.forwarding = forwarding;
    config.retransmission = retransmission;
    config.maxPlayers = maxPlayers;
    config.timeout = timeout;
    config.interval = interval;
    config.sendTimerId = sendTimerId;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library. When an adapter is connected, it changes the
   * state to `AUTHENTICATED`. It can also be used to disconnect or reset the
   * adapter.
   */
  bool activate() {
    LINK_READ_TAG(LINK_WIRELESS_VERSION);
    static_assert(LINK_WIRELESS_QUEUE_SIZE >= 1);
    static_assert(LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH >= 6 &&
                  LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH <= 21);
    static_assert(LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH >= 2 &&
                  LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH <= 4);

    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    lastError = Error::NONE;
    bool success = reset();

    LINK_BARRIER;
    isEnabled = true;
    LINK_BARRIER;

    return success;
  }

  /**
   * @brief Restores the state from an existing connection on the Wireless
   * Adapter hardware. This is useful, for example, after a fresh launch of a
   * Multiboot game, to synchronize the library with the current state and
   * avoid a reconnection. Returns whether the restoration was successful.
   * On success, the state should be either `SERVING` or `CONNECTED`.
   * \warning This should be used as a replacement for `activate()`.
   */
  bool restoreExistingConnection() {
    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    resetState();
    stopTimer();
    startTimer();

    if (!linkRawWireless.restoreExistingConnection() ||
        linkRawWireless.sessionState.playerCount > config.maxPlayers) {
      deactivate();
      return false;
    }

    LINK_BARRIER;
    isEnabled = true;
    LINK_BARRIER;

    return true;
  }

  /**
   * @brief Puts the adapter into a low consumption mode and then deactivates
   * the library. It returns a boolean indicating whether the transition to low
   * consumption mode was successful.
   * @param turnOff Whether the library should put the adapter in the low
   * consumption mode or not before deactivation. Defaults to `true`.
   */
  bool deactivate(bool turnOff = true) {
    bool success = true;

    if (turnOff)
      success = activate() && linkRawWireless.bye();

    lastError = Error::NONE;
    isEnabled = false;
    resetState();
    stop();

    return success;
  }

  /**
   * @brief Starts broadcasting a server and changes the state to `SERVING`. You
   * can optionally provide data that games will be able to read. If the adapter
   * is already serving, this method only updates the broadcast data.
   * @param gameName Game name. Maximum `14` characters + null terminator.
   * @param userName User name. Maximum `8` characters + null terminator.
   * @param gameId `(0 ~ 0x7FFF)` Game ID.
   * \warning Updating broadcast data while serving can fail if the adapter is
   * busy. In that case, this will return `false` and `getLastError()` will be
   * `BUSY_TRY_AGAIN`.
   */
  bool serve(const char* gameName = "",
             const char* userName = "",
             u16 gameId = LINK_WIRELESS_MAX_GAME_ID) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::AUTHENTICATED &&
        linkRawWireless.getState() != State::SERVING)
      return badRequest(Error::WRONG_STATE);
    if (Link::strlen(gameName) > LINK_WIRELESS_MAX_GAME_NAME_LENGTH)
      return badRequest(Error::GAME_NAME_TOO_LONG);
    if (Link::strlen(userName) > LINK_WIRELESS_MAX_USER_NAME_LENGTH)
      return badRequest(Error::USER_NAME_TOO_LONG);

    isSendingSyncCommand = true;
    if (isAsyncCommandActive())
      return badRequest(Error::BUSY_TRY_AGAIN);

    if (linkRawWireless.getState() != State::SERVING) {
      if (!setup(config.maxPlayers))
        return abort(Error::COMMAND_FAILED);
    }

    bool success = linkRawWireless.broadcast(gameName, userName, gameId, false);

    if (linkRawWireless.getState() != State::SERVING)
      success = success && linkRawWireless.startHost(false);

    if (!success)
      return abort(Error::COMMAND_FAILED);

    LINK_BARRIER;
    isSendingSyncCommand = false;
    LINK_BARRIER;

    return true;
  }

  /**
   * @brief Closes the server while keeping the session active, to prevent new
   * users from joining the room.
   * \warning This action can fail if the adapter is busy. In that case,
   * this will return `false` and `getLastError()` will be `BUSY_TRY_AGAIN`.
   */
  bool closeServer() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::SERVING ||
        linkRawWireless.sessionState.isServerClosed)
      return badRequest(Error::WRONG_STATE);

    isSendingSyncCommand = true;
    if (isAsyncCommandActive())
      return badRequest(Error::BUSY_TRY_AGAIN);

    LinkRawWireless::PollConnectionsResponse response;
    bool success = linkRawWireless.endHost(response);

    if (!success)
      return abort(Error::COMMAND_FAILED);

    LINK_BARRIER;
    isSendingSyncCommand = false;
    LINK_BARRIER;

    return true;
  }

  /**
   * @brief Retrieves the signal level of each player (0-255). For hosts, the
   * array will contain the signal level of each client in indexes 1-4. For
   * clients, it will only include the index corresponding to the
   * `currentPlayerId()`.
   * @param response A structure that will be filled with the signal levels.
   * \warning For clients, this action can fail if the adapter is busy. In that
   * case, this will return `false` and `getLastError()` will be
   * `BUSY_TRY_AGAIN`. For hosts, you already have this data, so it's free!
   */
  bool getSignalLevel(SignalLevelResponse& response) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive())
      return badRequest(Error::WRONG_STATE);

    if (linkRawWireless.getState() == LinkRawWireless::State::SERVING) {
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++)
        response.signalLevels[i] = sessionState.signalLevel.level[i];
      return true;
    }

    isSendingSyncCommand = true;
    if (isAsyncCommandActive())
      return badRequest(Error::BUSY_TRY_AGAIN);

    bool success = linkRawWireless.getSignalLevel(response);

    if (!success)
      return abort(Error::COMMAND_FAILED);

    LINK_BARRIER;
    isSendingSyncCommand = false;
    LINK_BARRIER;

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers.
   * @param servers The array to be filled with data.
   * @param serverCount The number to be filled with the number of found
   * servers.
   * \warning This action takes 1 second to complete.
   * \warning For an async version, see `getServersAsyncStart()`.
   */
  bool getServers(Server servers[], u32& serverCount) {
    return getServers(servers, serverCount, []() {});
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers.
   * @param servers The array to be filled with data.
   * @param serverCount The number to be filled with the number of found
   * servers.
   * @param onWait A function which will be invoked each time VBlank starts.
   * \warning This action takes 1 second to complete.
   * \warning For an async version, see `getServersAsyncStart()`.
   */
  template <typename F>
  bool getServers(Server servers[], u32& serverCount, F onWait) {
    serverCount = 0;

    if (!getServersAsyncStart())
      return false;

    waitVBlanks(BROADCAST_SEARCH_WAIT_FRAMES, onWait);

    if (!getServersAsyncEnd(servers, serverCount))
      return false;

    return true;
  }

  /**
   * @brief Starts looking for broadcasting servers and changes the state to
   * `SEARCHING`. After this, call `getServersAsyncEnd(...)` 1 second later.
   */
  bool getServersAsyncStart() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::AUTHENTICATED)
      return badRequest(Error::WRONG_STATE);

    bool success = linkRawWireless.broadcastReadStart();

    if (!success)
      return abort(Error::COMMAND_FAILED);

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers. Changes the state to `AUTHENTICATED` again.
   * @param servers The array to be filled with data.
   * @param serverCount The number to be filled with the number of found
   * servers.
   */
  bool getServersAsyncEnd(Server servers[], u32& serverCount) {
    serverCount = 0;

    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::SEARCHING)
      return badRequest(Error::WRONG_STATE);

    LinkRawWireless::BroadcastReadPollResponse response;
    bool success1 = linkRawWireless.broadcastReadPoll(response);

    if (!success1)
      return abort(Error::COMMAND_FAILED);

    bool success2 = linkRawWireless.broadcastReadEnd();

    if (!success2)
      return abort(Error::COMMAND_FAILED);

    auto foundServers = response.servers;
    for (u32 i = 0; i < response.serversSize; i++) {
      Server server;
      server.id = foundServers[i].id;
      server.gameId = foundServers[i].gameId;
      for (u32 j = 0; j < LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1; j++)
        server.gameName[j] = foundServers[i].gameName[j];
      for (u32 j = 0; j < LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1; j++)
        server.userName[j] = foundServers[i].userName[j];
      u8 nextClientNumber = foundServers[i].nextClientNumber;
      server.currentPlayerCount =
          nextClientNumber == 0xFF ? 0 : 1 + nextClientNumber;
      servers[i] = server;
    }
    serverCount = response.serversSize;

    return true;
  }

  /**
   * @brief Starts a connection with `serverId` and changes the state to
   * `CONNECTING`.
   * @param serverId Device ID of the server.
   */
  bool connect(u16 serverId) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::AUTHENTICATED)
      return badRequest(Error::WRONG_STATE);

    bool success = linkRawWireless.connect(serverId);

    if (!success)
      return abort(Error::COMMAND_FAILED);

    return true;
  }

  /**
   * @brief When connecting, this needs to be called until the state is
   * `CONNECTED`. It assigns a player ID. Keep in mind that `isConnected()` and
   * `playerCount()` won't be updated until the first message from the server
   * arrives.
   */
  bool keepConnecting() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::CONNECTING)
      return badRequest(Error::WRONG_STATE);

    LinkRawWireless::ConnectionStatus response;
    bool success1 = linkRawWireless.keepConnecting(response);

    if (!success1)
      return abort(Error::COMMAND_FAILED);

    if (response.phase == LinkRawWireless::ConnectionPhase::STILL_CONNECTING)
      return true;
    else if (response.phase == LinkRawWireless::ConnectionPhase::ERROR)
      return abort(Error::COMMAND_FAILED);

    auto success2 = linkRawWireless.finishConnection();
    if (!success2)
      return abort(Error::COMMAND_FAILED);

    return true;
  }

  /**
   * @brief Returns whether a `send(...)` call would fail due to the queue being
   * full or not.
   */
  bool canSend() { return !sessionState.newOutgoingMessages.isFull(); }

  /**
   * @brief Enqueues `data` to be sent to other nodes.
   * @param data The value to be sent.
   */
  bool send(u16 data) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive())
      return badRequest(Error::WRONG_STATE);

    if (!canSend()) {
      lastError = Error::BUFFER_IS_FULL;
      return false;
    }

    Message message;
    message.playerId = linkRawWireless.sessionState.currentPlayerId;
    message.data = data;

    sessionState.newOutgoingMessages.syncPush(message);

    return true;
  }

  /**
   * @brief Fills the `messages` array with incoming messages.
   * @param messages The array to be filled with data.
   * @param receivedCount The number to be filled with the number of received
   * messages.
   */
  bool receive(Message messages[], u32& receivedCount) {
    receivedCount = 0;

    if (!isSessionActive())
      return false;

    LINK_BARRIER;
    sessionState.incomingMessages.startReading();
    LINK_BARRIER;

    while (!sessionState.incomingMessages.isEmpty()) {
      auto message = sessionState.incomingMessages.pop();
      if (message.playerId < LINK_WIRELESS_MAX_PLAYERS) {
        messages[receivedCount] = message;
        receivedCount++;
      }
    }

    LINK_BARRIER;
    sessionState.incomingMessages.stopReading();
    LINK_BARRIER;

    return true;
  }

  /**
   * @brief Returns the current state.
   * @return One of the enum values from `State`.
   */
  [[nodiscard]] State getState() { return linkRawWireless.getState(); }

  /**
   * @brief Returns `true` if the player count is higher than `1`.
   */
  [[nodiscard]] bool isConnected() {
    return linkRawWireless.sessionState.playerCount > 1;
  }

  /**
   * @brief Returns `true` if the state is `SERVING` or `CONNECTED`.
   */
  [[nodiscard]] bool isSessionActive() {
    return linkRawWireless.getState() == State::SERVING ||
           linkRawWireless.getState() == State::CONNECTED;
  }

  /**
   * @brief Returns `true` if the server was closed with `closeServer()`.
   */
  [[nodiscard]] bool isServerClosed() {
    return linkRawWireless.sessionState.isServerClosed;
  }

  /**
   * @brief Returns the number of connected players (`1~5`).
   */
  [[nodiscard]] u8 playerCount() {
    return linkRawWireless.sessionState.playerCount;
  }

  /**
   * @brief Returns the current player ID (`0~4`).
   */
  [[nodiscard]] u8 currentPlayerId() {
    return linkRawWireless.sessionState.currentPlayerId;
  }

  /**
   * @brief Returns whether the internal queue lost messages at some point due
   * to being full. This can happen if your queue size is too low, if you
   * receive too much data without calling `receive(...)` enough times, or if
   * excessive `receive(...)` calls prevent the ISR from copying data. After
   * this call, the overflow flag is cleared if `clear` is `true` (default
   * behavior).
   */
  bool didQueueOverflow(bool clear = true) {
    bool overflowReceive = sessionState.newIncomingMessages.overflow;
    bool overflowForwardedMessage = sessionState.outgoingMessages.overflow;
    if (clear) {
      sessionState.newIncomingMessages.overflow = false;
      sessionState.outgoingMessages.overflow = false;
    }
    return overflowReceive || overflowForwardedMessage;
  }

  /**
   * @brief Resets other players' timeout count to `0`.
   * \warning Call this if you changed `config.timeout`.
   */
  void resetTimeout() {
    if (!isEnabled)
      return;

    LINK_BARRIER;
    sessionState.isResetTimeoutPending = true;
    LINK_BARRIER;
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
   * @brief If one of the other methods returns `false`, you can inspect this to
   * know the cause. After this call, the last error is cleared if `clear` is
   * `true` (default behavior).
   * @param clear Whether it should clear the error or not.
   */
  Error getLastError(bool clear = true) {
    Error error = lastError;
    if (clear)
      lastError = Error::NONE;
    return error;
  }

  /**
   * @brief Returns the number of total outgoing messages.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _getPendingCount() {
    return sessionState.outgoingMessages.size();
  }

  /**
   * @brief Returns the number of inflight outgoing messages.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _getInflightCount() { return sessionState.inflightCount; }

  /**
   * @brief Returns the number of forwarded outgoing messages.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _getForwardedCount() { return sessionState.forwardedCount; }

  /**
   * @brief Returns the last packet ID.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketId() { return sessionState.lastPacketId; }

  /**
   * @brief Returns the last ACK received from player ID 1.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastAckFromClient1() {
    return sessionState.lastAckFromClients[1];
  }

  /**
   * @brief Returns the last packet ID received from player ID 1.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketIdFromClient1() {
    return sessionState.lastPacketIdFromClients[1];
  }

  /**
   * @brief Returns the last ACK received from the server.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastAckFromServer() {
    return sessionState.lastAckFromServer;
  }

  /**
   * @brief Returns the last packet ID received from the server.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketIdFromServer() {
    return sessionState.lastPacketIdFromServer;
  }

  /**
   * @brief Returns the next pending packet ID.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _nextPendingPacketId() {
    return sessionState.outgoingMessages.isEmpty()
               ? 0
               : sessionState.outgoingMessages.peek().packetId;
  }
#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  /**
   * @brief Sets a logger function.
   * \warning This is internal API!
   */
  void _setLogger(LinkRawWireless::Logger logger) {
    linkRawWireless.logger = logger;
  }
#endif

  /**
   * @brief This method is called by the VBLANK interrupt handler.
   * \warning This is internal API!
   */
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  LINK_NOINLINE void _onVBlank() {
#else
  void _onVBlank() {
#endif
    if (!isEnabled)
      return;

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    if (interrupt) {
      pendingVBlank = true;
      return;
    }
#endif

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    profileStart();
#endif

    if (!isSessionActive())
      return;

    if (sessionState.isResetTimeoutPending) {
      sessionState.recvTimeout = 0;
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++)
        sessionState.msgTimeouts[i] = 0;
      sessionState.isResetTimeoutPending = false;
    }

    if (isConnected() && !sessionState.recvFlag)
      sessionState.recvTimeout++;
    if (sessionState.recvTimeout >= config.timeout)
      return (void)abort(Error::TIMEOUT);

    if (!checkRemoteTimeouts())
      return (void)abort(Error::REMOTE_TIMEOUT);

    sessionState.recvFlag = false;
    sessionState.signalLevelCalled = false;

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    vblankTime += profileStop();
    vblankIRQs++;
#endif
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  LINK_WIRELESS_ISR_FUNC(_onSerial, (), (), {
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    interrupt = true;
    LINK_BARRIER;
    // (nested interrupts are enabled by LinkRawWireless::_onSerial(...))
#endif

    ___onSerial();

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    irqEnd();
#endif
  })

  /**
   * @brief This method is called by the TIMER interrupt handler.
   * \warning This is internal API!
   */
  LINK_WIRELESS_ISR_FUNC(_onTimer, (), (), {
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    if (interrupt)
      return;

    interrupt = true;
    LINK_BARRIER;
    Link::_REG_IME = 1;
#endif

    ___onTimer();

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    irqEnd();
#endif
  })

  struct Config {
    bool forwarding;
    bool retransmission;
    u8 maxPlayers;
    u32 timeout;   // can be changed in realtime, but call `resetTimeout()`
    u16 interval;  // can be changed in realtime, but call `resetTimer()`
    u8 sendTimerId;
  };

  /**
   * @brief LinkWireless configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

#ifndef LINK_WIRELESS_DEBUG_MODE
 private:
#endif
  using MessageQueue = Link::Queue<Message, LINK_WIRELESS_QUEUE_SIZE>;

  struct SignalLevel {
    vu8 level[LINK_WIRELESS_MAX_PLAYERS] = {};
  };

  struct SessionState {
    MessageQueue incomingMessages;     // read by user, write by irq&user
    MessageQueue outgoingMessages;     // read and write by irq
    MessageQueue newIncomingMessages;  // read and write by irq
    MessageQueue newOutgoingMessages;  // read by irq, write by user&irq
    SignalLevel signalLevel;           // write by irq, read by any

    u32 recvTimeout = 0;                         // (~= LinkCable::IRQTimeout)
    u32 msgTimeouts[LINK_WIRELESS_MAX_PLAYERS];  // (~= LinkCable::msgTimeouts)
    bool recvFlag = false;                       // (~= LinkCable::IRQFlag)
    bool msgFlags[LINK_WIRELESS_MAX_PLAYERS];    // (~= LinkCable::msgFlags)

    bool signalLevelCalled = false;
    bool sendReceiveLatch = false;  // true = send ; false = receive
    bool shouldWaitForServer = false;

    bool didReceiveFirstPacketFromServer = false;
    u32 inflightCount = 0;
    u32 forwardedCount = 0;
    u32 lastPacketId = 0;
    u32 lastPacketIdFromServer = 0;
    u32 lastAckFromServer = 0;
    u32 lastPacketIdFromClients[LINK_WIRELESS_MAX_PLAYERS];
    u32 lastAckFromClients[LINK_WIRELESS_MAX_PLAYERS];
    int lastHeartbeatFromClients[LINK_WIRELESS_MAX_PLAYERS];
    int localHeartbeat = -1;
    volatile bool isResetTimeoutPending = false;
  };

  struct TransferHeader {
    // - This header is appended as the first word of every transfer.
    // - Packets ("messages") are 16-bit.
    // - Packet IDs are 0~63 (server) and 0~15 (clients).
    // - They wrap around, so 31 and 7 are the maximum number of inflight
    //   packets as per the N/2-1 rule.
    // - Messages are sent in order and retransmitted until their ACK. e.g.:
    //   * >> 1, 2, 3
    //   * >> 1, 2, 3, 4, 5
    //   * << ack=3
    //   * >> 4, 5, 6
    //   This wastes bandwidth but reduces latency, since waiting for a
    //   retransmission until not receiving an ACK takes time, and games usually
    //   care more about latency than bandwidth.
    // - The first message can be in the header itself (bits 0~15) when:
    //   * (there *is* something to send) && (it's from a client)
    //   * -> this is indicated with a 1 in `firstPacketId`'s bit 4
    // - Each of the next words in the transfer contain two 16-bit messages.
    //   * Low part first, high part last
    //   * The last word can contain 1 or 2 messages depending on `hasLastMsg`
    // - When `forwarding` is enabled, the server can forward messages from
    //   other clients. If the stream includes forwarded messages, this header
    //   contains `hasPlayerBitMap`=1, and the next halfword is a
    //   `PlayerBitMap`.
    unsigned int _reserved_ : 4;  // unused (or first msg!)
    unsigned int ack4 : 4;        // server: player 4 ACK (or first msg!)
    unsigned int ack3 : 4;        // server: player 3 ACK (or first msg!)
    unsigned int ack2 : 4;        // server: player 2 ACK (or first msg!)
    unsigned int ack1 : 6;        // server: player 1 ACK
                                  // clients: server ACK
    unsigned int hasLastMsg : 1;  // there's a msg in last word's high part
    unsigned int
        hasPlayerBitMap : 1;         // server: next halfword is a PlayerBitMap
    unsigned int firstPacketId : 6;  // next packets are assumed consecutive
                                     // clients only use 4 bits here!
                                     // `hasFirstMsg` is an imaginary flag
                                     // living in `firstPacketId`'s bit 4
    unsigned int playerCount : 2;    // server: playerCount (0 = 2; ...; 3 = 5)
                                     // clients: heartbeat (0~3)
  };

  struct PlayerBitMap {
    // - This halfword is appended after the `TransferHeader` if the stream
    // contains messages from multiple users (when `forwarding` is enabled).
    // - `playerIds` is a bit-packed array of 5 3-bit entries, representing the
    //    owners of the next 5 messages (bits 0~2 = first message's player ID).
    // - `hasMore` indicates whether there are more messages owned by player IDs
    //   greater than 0, which adds another `PlayerBitMap` after the messages.
    // - Each `PlayerBitMap` occupies the space of a regular message, but
    //   doesn't affect the packet ID sequence. By design, they are always
    //   placed in the low part.
    // ---
    // Example: Let's say there's a stream with 12 packets (p1~pC).
    // Before pB, some of the packets are forwarded from other clients.
    // So, the transfer header (THD) takes the first word and sets
    // `hasPlayerBitMap=1`. The low part of the next word is a `PlayerBitMap`
    // (PB) and describes the owners of the following 5 messages (p1~p5). Since
    // there are more forwarded messages (last PB had `hasMore`=1), a new PB is
    // added describing p6~pA. The last two messages (pB~pC) are server
    // messages, so there's no need for a new PB (last PB had `hasMore`=0).
    //   w00    w01    w02    w03    w04    w05    w06    w07
    // |-----||--|--||--|--||--|--||--|--||--|--||--|--||--|--|
    // | THD ||p1|PB||p3|p2||p5|p4||p6|PB||p8|p7||pA|p9||pC|pB|
    // |-----||--|--||--|--||--|--||--|--||--|--||--|--||--|--|
    // ---
    unsigned int playerIds : 15;  // 5 entries, 3 bits per player
    unsigned int hasMore : 1;  // if true, there's another `PlayerBitMap` after
                               // the next 5 messages
  };

  template <typename H>
  union U32Packer {
    H asStruct;
    u32 asInt;
  };

  using CommandResult = LinkRawWireless::CommandResult;

  LinkRawWireless linkRawWireless;
  SessionState sessionState;
  u32 nextAsyncCommandData[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
  u32 nextAsyncCommandDataSize = 0;
  volatile bool isSendingSyncCommand = false;
  volatile Error lastError = Error::NONE;
  volatile bool isEnabled = false;

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  volatile bool interrupt = false, pendingVBlank = false;

  LINK_INLINE void irqEnd() {
    Link::_REG_IME = 0;
    interrupt = false;
    LINK_BARRIER;
    if (pendingVBlank) {
      _onVBlank();
      pendingVBlank = false;
    }
  }
#endif

  LINK_INLINE void ___onSerial() {
    if (!isEnabled)
      return;

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    profileStart();
#endif

    int status = linkRawWireless._onSerial(false);
    if (status <= -4) {
      return (void)abort(Error::ACKNOWLEDGE_FAILED);
    } else if (status > 0) {
      auto result = linkRawWireless._getAsyncCommandResultRef();
      processAsyncCommand(result);
    }

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    serialTime += profileStop();
    serialIRQs++;
#endif
  }

  LINK_INLINE void ___onTimer() {
    if (!isEnabled)
      return;

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    profileStart();
#endif

    if (!isSessionActive())
      return;

    if (!isAsyncCommandActive())
      checkConnectionsOrTransferData();

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    timerTime += profileStop();
    timerIRQs++;
#endif
  }

  LINK_INLINE void processAsyncCommand(
      const LinkRawWireless::CommandResult* commandResult) {  // (irq only)
    if (!commandResult->success) {
      return (void)abort(
          commandResult->commandId == LinkRawWireless::COMMAND_SEND_DATA
              ? Error::SEND_DATA_FAILED
          : commandResult->commandId == LinkRawWireless::COMMAND_RECEIVE_DATA
              ? Error::RECEIVE_DATA_FAILED
              : Error::COMMAND_FAILED);
    }

    switch (commandResult->commandId) {
      case LinkRawWireless::COMMAND_SIGNAL_LEVEL: {
        // SignalLevel (end)
        u32 levels = commandResult->dataSize > 0 ? commandResult->data[0] : 0;
        u32 players = 1;
        for (u32 i = 1; i < LINK_WIRELESS_MAX_PLAYERS; i++) {
          u32 level = (levels >> ((i - 1) * 8)) & 0xFF;
          sessionState.signalLevel.level[i] = level;
          if (level > 0)
            players++;
        }

        if (players > linkRawWireless.sessionState.playerCount) {
          LINK_BARRIER;
          linkRawWireless.sessionState.playerCount =
              Link::_min(players, config.maxPlayers);
          LINK_BARRIER;
        }

        break;
      }
      case LinkRawWireless::COMMAND_SEND_DATA: {
        // SendData (end)

        if (linkRawWireless.getState() == State::CONNECTED)
          sessionState.shouldWaitForServer = true;
        sessionState.sendReceiveLatch = !sessionState.sendReceiveLatch;

        break;
      }
      case LinkRawWireless::COMMAND_RECEIVE_DATA: {
        // ReceiveData (end)

        sessionState.sendReceiveLatch =
            sessionState.shouldWaitForServer || !sessionState.sendReceiveLatch;

        if (commandResult->dataSize == 0)
          break;

        sessionState.recvFlag = true;
        sessionState.recvTimeout = 0;

        sessionState.shouldWaitForServer = false;

        addIncomingMessagesFromData(commandResult);

        break;
      }
      default: {
      }
    }
  }

  LINK_WIRELESS_TIMER_ISR void checkConnectionsOrTransferData() {  // (irq only)
    if (linkRawWireless.getState() == State::SERVING &&
        !sessionState.signalLevelCalled) {
      // SignalLevel (start)
      if (sendCommandAsync(LinkRawWireless::COMMAND_SIGNAL_LEVEL))
        sessionState.signalLevelCalled = true;
    } else if (linkRawWireless.getState() == State::CONNECTED ||
               isConnected()) {
      bool shouldReceive =
          !sessionState.sendReceiveLatch || sessionState.shouldWaitForServer;

      if (shouldReceive) {
        // ReceiveData (start)
        sendCommandAsync(LinkRawWireless::COMMAND_RECEIVE_DATA);
      } else {
        // SendData (start)
        sendPendingData();
      }
    }
  }

  LINK_WIRELESS_TIMER_ISR void sendPendingData() {  // (irq only)
    copyOutgoingState();

    setDataFromOutgoingMessages();
    if (sendCommandAsync(LinkRawWireless::COMMAND_SEND_DATA, true))
      clearInflightMessagesIfNeeded();
  }

  LINK_WIRELESS_TIMER_ISR void setDataFromOutgoingMessages() {  // (irq only)
    addAsyncData(0, true);  // SendData header (filled later)
    addAsyncData(0);        // Transfer header (filled later)

    bool isServer = linkRawWireless.getState() == State::SERVING;
    u32 maxPacketIds = isServer ? MAX_PACKET_IDS_SERVER : MAX_PACKET_IDS_CLIENT;
    u32 maxInflightPackets =
        isServer ? MAX_INFLIGHT_PACKETS_SERVER : MAX_INFLIGHT_PACKETS_CLIENT;
    u32 maxTransferLength = 1 + getDeviceTransferLength();
    // (+1 for SendData header)

    u32 firstPacketId = NO_ID_ASSIGNED_YET;
    u32 firstMsg = 0;
    u32 msgCount = 0;
    bool highPart = false;
    u32 pendingForwardedCount = sessionState.forwardedCount;
    int currentPlayerBitMapIndex = -1;
    u32 playerBitMapCount = 0;
    sessionState.outgoingMessages.forEach(
        [this, isServer, maxPacketIds, maxInflightPackets, maxTransferLength,
         &firstPacketId, &firstMsg, &msgCount, &highPart,
         &pendingForwardedCount, &currentPlayerBitMapIndex,
         &playerBitMapCount](Message* message) {
          // create packet ID if the packet can be sent
          if (message->packetId == NO_ID_ASSIGNED_YET) {
            if (sessionState.inflightCount < maxInflightPackets) {
              message->packetId = newPacketId(maxPacketIds);
              sessionState.inflightCount++;
            } else {
              return false;
            }
          }

          // get first added packet ID and add first msg if needed
          if (firstPacketId == NO_ID_ASSIGNED_YET) {
            firstPacketId = message->packetId;
            if (!isServer) {
              msgCount++;
              firstMsg = message->data;
              return true;
            }
          }

          // add a new PlayerBitMap if needed
          if (pendingForwardedCount > 0 &&
              (currentPlayerBitMapIndex < 0 ||
               playerBitMapCount == MAX_PLAYER_BITMAP_ENTRIES)) {
            if (playerBitMapCount == MAX_PLAYER_BITMAP_ENTRIES)
              addToAsyncDataShifted(currentPlayerBitMapIndex, 1, BIT_HAS_MORE);

            // `highPart` should always be false here!
            currentPlayerBitMapIndex = nextAsyncCommandDataSize;
            playerBitMapCount = 0;
            addAsyncData(0);
            highPart = true;
          }

          // add to the correct part of the u32
          if (highPart)
            addToLastAsyncDataHalfword(message->data);
          else
            addAsyncData(message->data);
          highPart = !highPart;

          // update player bitmap if needed
          if (currentPlayerBitMapIndex >= 0) {
            addToAsyncDataShifted(currentPlayerBitMapIndex, message->playerId,
                                  PLAYER_ID_BITS * playerBitMapCount);
            playerBitMapCount++;

            if (message->playerId > 0) {
              pendingForwardedCount--;
              if (pendingForwardedCount == 0)
                currentPlayerBitMapIndex = -1;
            }
          }

          msgCount++;

          // only continue if we have available halfwords
          return nextAsyncCommandDataSize < maxTransferLength || highPart;
        });

    // fill Transfer header
    nextAsyncCommandData[1] = buildTransferHeader(isServer, firstPacketId,
                                                  firstMsg, msgCount, highPart);

    // fill SendData header
    u32 bytes = (nextAsyncCommandDataSize - 1) * 4;
    nextAsyncCommandData[0] = linkRawWireless.getSendDataHeaderFor(bytes);
  }

  u32 buildTransferHeader(bool isServer,
                          u32 firstPacketId,
                          u32 firstMsg,
                          u32 msgCount,
                          bool highPart) {  // (irq only)
    TransferHeader transferHeader = {};

    // player count / client heartbeat
    if (isServer) {
      transferHeader.playerCount =
          linkRawWireless.sessionState.playerCount - LINK_WIRELESS_MIN_PLAYERS;
    } else {
      sessionState.localHeartbeat = (sessionState.localHeartbeat + 1) % 4;
      transferHeader.playerCount = sessionState.localHeartbeat;
    }

    // first packet ID, or 0 if there are no messages
    if (msgCount > 0)
      transferHeader.firstPacketId = firstPacketId;

    // the last word has a message in its high part if there are messages and we
    // fully used all words
    transferHeader.hasLastMsg = msgCount > 0 && !highPart;

    // the transfer contains player bitmaps if there are forwarded messages
    transferHeader.hasPlayerBitMap = sessionState.forwardedCount > 0;

    // `ack1` is used by both server and clients
    transferHeader.ack1 = isServer ? sessionState.lastPacketIdFromClients[1]
                                   : sessionState.lastPacketIdFromServer;

    if (isServer) {
      // server use `ack2`, `ack3` and `ack4` for acknowledging P2, P3, P4
      transferHeader.ack2 = sessionState.lastPacketIdFromClients[2];
      transferHeader.ack3 = sessionState.lastPacketIdFromClients[3];
      transferHeader.ack4 = sessionState.lastPacketIdFromClients[4];
    } else if (msgCount > 0) {
      // but clients can use this area for storing the first message (*)
      transferHeader.firstPacketId |= HAS_FIRST_MSG_MASK;
    }

    // interpret the whole thing as u32
    U32Packer<TransferHeader> packer = {};
    packer.asStruct = transferHeader;
    if (!isServer)
      packer.asInt |= firstMsg;  // (*)
    return packer.asInt;
  }

  void clearInflightMessagesIfNeeded() {  // (irq only)
    if (config.retransmission)
      return;

    while (!sessionState.outgoingMessages.isEmpty()) {
      u32 packetId = sessionState.outgoingMessages.peek().packetId;
      if (packetId == NO_ID_ASSIGNED_YET)
        break;

      auto message = sessionState.outgoingMessages.pop();
      if (linkRawWireless.getState() == State::SERVING && message.playerId > 0)
        sessionState.forwardedCount--;
    }

    sessionState.inflightCount = 0;
  }

  LINK_WIRELESS_SERIAL_ISR void addIncomingMessagesFromData(
      const CommandResult* result) {  // (irq only)
    // parse ReceiveData header
    u32 sentBytes[LINK_WIRELESS_MAX_PLAYERS] = {0, 0, 0, 0, 0};
    u32 receiveDataHeader = result->data[0];
    sentBytes[0] = Link::_min(receiveDataHeader & 0b1111111,
                              LinkRawWireless::MAX_TRANSFER_BYTES_SERVER);
    sentBytes[1] = Link::_min((receiveDataHeader >> 8) & 0b11111,
                              LinkRawWireless::MAX_TRANSFER_BYTES_CLIENT);
    sentBytes[2] = Link::_min((receiveDataHeader >> 13) & 0b11111,
                              LinkRawWireless::MAX_TRANSFER_BYTES_CLIENT);
    sentBytes[3] = Link::_min((receiveDataHeader >> 18) & 0b11111,
                              LinkRawWireless::MAX_TRANSFER_BYTES_CLIENT);
    sentBytes[4] = Link::_min((receiveDataHeader >> 23) & 0b11111,
                              LinkRawWireless::MAX_TRANSFER_BYTES_CLIENT);

    bool isServer = linkRawWireless.getState() == State::SERVING;
    u32 cursor = 1;
    u32 startPlayerId = isServer ? 1 : 0;
    u32 endPlayerId = isServer ? linkRawWireless.sessionState.playerCount : 1;

    // server reads from indexes 1~4, clients read from index 0
    for (u32 i = startPlayerId; i < endPlayerId; i++) {
      if (sentBytes[i] % 4 != 0)
        return;  // in our protocol, we always send whole words!

      u32 remainingWords = sentBytes[i] / 4;
      if (remainingWords == 0)
        continue;

      // parse TransferHeader
      U32Packer<TransferHeader> packer;
      packer.asInt = result->data[cursor++];
      remainingWords--;
      TransferHeader header = packer.asStruct;

      // if retransmission is enabled, we update the confirmations based on the
      // ACKs found in the header
      if (config.retransmission) {
        if (isServer) {
          sessionState.lastAckFromClients[i] = header.ack1;
        } else {
          u32 currentPlayerId = linkRawWireless.sessionState.currentPlayerId;
          sessionState.lastAckFromServer = currentPlayerId == 1   ? header.ack1
                                           : currentPlayerId == 2 ? header.ack2
                                           : currentPlayerId == 3 ? header.ack3
                                                                  : header.ack4;
        }
      }

      // clients update their player count based on the transfer header
      if (!isServer) {
        LINK_BARRIER;
        linkRawWireless.sessionState.playerCount =
            LINK_WIRELESS_MIN_PLAYERS + header.playerCount;
        LINK_BARRIER;
      }

      // clients can send their first message in the header itself
      u32 currentPacketId = header.firstPacketId;
      bool hasFirstMsg =
          isServer && (currentPacketId & HAS_FIRST_MSG_MASK) != 0;
      if (hasFirstMsg) {
        currentPacketId &= ~HAS_FIRST_MSG_MASK;
        u32 playerBitMap = 0;
        int playerBitMapCount = -1;
        processMessage(i, Link::lsB32(packer.asInt), currentPacketId,
                       playerBitMap, playerBitMapCount);
      }

      // process the remaining words as message pairs
      u32 playerBitMap = 0;
      int playerBitMapCount =
          !isServer && header.hasPlayerBitMap ? MAX_PLAYER_BITMAP_ENTRIES : -1;
      while (remainingWords > 0) {
        bool hasHighPart = remainingWords > 1 || header.hasLastMsg;

        u32 word = result->data[cursor];
        u32 lowPart = Link::lsB32(word);
        if (playerBitMapCount >= MAX_PLAYER_BITMAP_ENTRIES) {
          playerBitMap = lowPart;
          playerBitMapCount = 0;
        } else
          processMessage(i, lowPart, currentPacketId, playerBitMap,
                         playerBitMapCount);

        if (hasHighPart) {
          u32 highPart = Link::msB32(word);
          processMessage(i, highPart, currentPacketId, playerBitMap,
                         playerBitMapCount);
        }

        cursor++;
        remainingWords--;
      }

      bool shouldResetTimeouts = true;
      if (isServer) {
        // reset timeouts, only if the heartbeat from the clients changed (*)
        int heartbeat = header.playerCount;
        shouldResetTimeouts =
            heartbeat != sessionState.lastHeartbeatFromClients[i];
        sessionState.lastHeartbeatFromClients[i] = heartbeat;
        // (*) sometimes, when a client is disconnected, the Wireless Adapter
        // keeps repeating old data in its slot! we use this heartbeat to verify
        // that the client is still generating packets actively!
      }

      if (shouldResetTimeouts) {
        sessionState.msgTimeouts[0] = 0;
        sessionState.msgTimeouts[i] = 0;
        sessionState.msgFlags[0] = true;
        sessionState.msgFlags[i] = true;
      }
    }

    // remove confirmed messages based on the updated ACKs
    if (config.retransmission) {
      if (isServer)
        removeConfirmedMessagesFromClients();
      else
        removeConfirmedMessagesFromServer();
    }

    // copy data from the interrupt world to the main world
    copyIncomingState();
  }

  LINK_WIRELESS_ISR_FUNC(
      processMessage,
      (u32 playerId,
       u32 data,
       u32& currentPacketId,
       u32& playerBitMap,
       int& playerBitMapCount),
      (playerId, data, currentPacketId, playerBitMap, playerBitMapCount),
      {
        // (irq only)
        // store the packet ID and increment (msgs are consecutive inside
        // transfers)
        u32 packetId = currentPacketId;
        currentPacketId =
            (currentPacketId + 1) %
            (playerId == 0 ? MAX_PACKET_IDS_SERVER : MAX_PACKET_IDS_CLIENT);

        // get msg player ID based on player bitmap
        u32 msgPlayerId = playerId;
        if (playerBitMapCount >= 0) {
          msgPlayerId = (playerBitMap >> PLAYER_ID_BITS * playerBitMapCount) &
                        PLAYER_ID_MASK;
          playerBitMapCount++;
          // (messages from remote player IDs 5, 6 and 7 could be received here,
          // but it's fine because `receive(...)` filters invalid entries)

          if (playerBitMapCount >= MAX_PLAYER_BITMAP_ENTRIES &&
              !((playerBitMap >> BIT_HAS_MORE) & 1))
            playerBitMapCount = -1;
        }

        if (playerId == 0 && !sessionState.didReceiveFirstPacketFromServer) {
          // the first time clients receive something from the server,
          // they shouldn't have any expectations (since they can join at any
          // time)
          sessionState.lastPacketIdFromServer = packetId;
          sessionState.didReceiveFirstPacketFromServer = true;
        } else {
          // if retransmission is enabled, the packet ID needs to be expected
          if (config.retransmission) {
            u32 expectedPacketId =
                playerId > 0
                    ? (sessionState.lastPacketIdFromClients[playerId] + 1) %
                          MAX_PACKET_IDS_CLIENT
                    : (sessionState.lastPacketIdFromServer + 1) %
                          MAX_PACKET_IDS_SERVER;

            if (packetId != expectedPacketId)
              return;

            if (playerId > 0)
              sessionState.lastPacketIdFromClients[playerId] = expectedPacketId;
            else
              sessionState.lastPacketIdFromServer = expectedPacketId;
          }
        }

        // ignore messages from myself
        if (msgPlayerId == linkRawWireless.sessionState.currentPlayerId)
          return;

        // add new message
        Message message;
        message.playerId = msgPlayerId;
        message.data = data;
        message.packetId = packetId;
        sessionState.newIncomingMessages.push(message);

        // forward to other clients if needed
        if (playerId > 0 && config.forwarding &&
            linkRawWireless.sessionState.playerCount > 2)
          forwardMessage(message);
      })

  LINK_WIRELESS_SERIAL_ISR void forwardMessage(
      Message& message) {  // (irq only)
    Message forwardedMessage;
    forwardedMessage.data = message.data;
    forwardedMessage.playerId = message.playerId;
    if (!sessionState.outgoingMessages.isFull()) {
      sessionState.outgoingMessages.push(forwardedMessage);
      sessionState.forwardedCount++;
    } else
      sessionState.outgoingMessages.overflow = true;
  }

  LINK_WIRELESS_SERIAL_ISR void
  removeConfirmedMessagesFromServer() {  // (irq only)
    removeConfirmedMessages(sessionState.lastAckFromServer,
                            MAX_PACKET_IDS_CLIENT, MAX_INFLIGHT_PACKETS_CLIENT);
  }

  LINK_WIRELESS_SERIAL_ISR void
  removeConfirmedMessagesFromClients() {  // (irq only)
    u32 ringMinAck = 0xFFFFFFFF;
    for (u32 i = 1; i < linkRawWireless.sessionState.playerCount; i++) {
      u32 ack = sessionState.lastAckFromClients[i];

      // ignore clients that didn't confirm anything yet
      if (ack == NO_ACK_RECEIVED_YET)
        continue;

      if (ringMinAck == 0xFFFFFFFF) {
        // on first time, we set `ringMinAck`
        ringMinAck = ack;
      } else {
        // we compare `ringMinAck` vs `ack` in circular space
        // (0..MAX_PACKET_IDS_CLIENT-1):
        //   -> how many steps it is from `ringMinAck` down to `ack`?
        u32 dist = (ringMinAck - ack) & (MAX_PACKET_IDS_CLIENT - 1);

        // if dist >= MAX_INFLIGHT_PACKETS_CLIENT => `ack` is "behind"
        // `ringMinAck`, so we replace it!
        if (dist >= MAX_INFLIGHT_PACKETS_CLIENT)
          ringMinAck = ack;
      }
    }

    // if we found a valid minimum ack across all clients, we remove!
    if (ringMinAck != 0xFFFFFFFF)
      removeConfirmedMessages(ringMinAck, MAX_PACKET_IDS_SERVER,
                              MAX_INFLIGHT_PACKETS_SERVER);
  }

  LINK_WIRELESS_SERIAL_ISR void removeConfirmedMessages(
      u32 ack,
      const u32 maxPacketIds,
      const u32 maxInflightPackets) {  // (irq only)
    while (!sessionState.outgoingMessages.isEmpty()) {
      u32 packetId = sessionState.outgoingMessages.peek().packetId;

      // if the current message is not inflight, we've entered the section of
      // 'new' messages (with no ID assigned), so we quit!
      if (packetId == NO_ID_ASSIGNED_YET)
        break;

      // we release the packet if it was confirmed (aka inside the send window)
      // example with maxPacketIds=16, maxInflightPackets=7, ack=4:
      //   => we would be releasing packets 4,3,2,1,15,14,13
      if (((ack - packetId) & (maxPacketIds - 1)) <= maxInflightPackets) {
        auto message = sessionState.outgoingMessages.pop();
        sessionState.inflightCount--;
        if (maxPacketIds == MAX_PACKET_IDS_SERVER && message.playerId > 0)
          sessionState.forwardedCount--;
      } else
        break;
    }
  }

  LINK_WIRELESS_TIMER_ISR u32 getDeviceTransferLength() {  // (irq only)
    return linkRawWireless.getState() == State::SERVING
               ? LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
               : LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH;
  }

  LINK_WIRELESS_TIMER_ISR void copyOutgoingState() {  // (irq only)
    if (sessionState.newOutgoingMessages.isWriting())
      return;

    while (!sessionState.newOutgoingMessages.isEmpty() &&
           !sessionState.outgoingMessages.isFull()) {
      auto message = sessionState.newOutgoingMessages.pop();
      sessionState.outgoingMessages.push(message);
    }
  }

  LINK_WIRELESS_SERIAL_ISR void copyIncomingState() {  // (irq only)
    if (sessionState.incomingMessages.isReading())
      return;

    while (!sessionState.newIncomingMessages.isEmpty() &&
           !sessionState.incomingMessages.isFull()) {
      auto message = sessionState.newIncomingMessages.pop();
      sessionState.incomingMessages.push(message);
    }
  }

  bool checkRemoteTimeouts() {  // (irq only)
    bool isServer = linkRawWireless.getState() == State::SERVING;
    u32 startPlayerId = isServer ? 1 : 0;
    u32 endPlayerId = isServer ? linkRawWireless.sessionState.playerCount : 1;

    for (u32 i = startPlayerId; i < endPlayerId; i++) {
      if (!sessionState.msgFlags[i]) {
        sessionState.msgTimeouts[i]++;
        if (sessionState.msgTimeouts[i] > config.timeout)
          return false;
      }
      sessionState.msgFlags[i] = false;
    }

    return true;
  }

  LINK_WIRELESS_TIMER_ISR u32 newPacketId(u32 maxPacketIds) {  // (irq only)
    return (sessionState.lastPacketId =
                (sessionState.lastPacketId + 1) % maxPacketIds);
  }

  LINK_WIRELESS_TIMER_ISR void addToLastAsyncDataHalfword(
      u16 value) {  // (irq only)
    addToAsyncDataShifted(nextAsyncCommandDataSize - 1, value, 16);
  }

  LINK_WIRELESS_TIMER_ISR void addToAsyncDataShifted(u32 index,
                                                     u16 value,
                                                     u32 shift) {  // (irq only)
    nextAsyncCommandData[index] |= value << shift;
  }

  LINK_WIRELESS_TIMER_ISR void addAsyncData(u32 value,
                                            bool start = false) {  // (irq only)
    if (start)
      nextAsyncCommandDataSize = 0;
    nextAsyncCommandData[nextAsyncCommandDataSize] = value;
    nextAsyncCommandDataSize++;
  }

  bool sendCommandAsync(u8 type,
                        bool withData = false) {  // (irq only)
    if (isSendingSyncCommand)
      return false;

    u32 size = withData ? nextAsyncCommandDataSize : 0;
    return linkRawWireless.sendCommandAsync(type, nextAsyncCommandData, size,
                                            false, true);
  }

  bool isAsyncCommandActive() {
    return linkRawWireless.getAsyncState() ==
           LinkRawWireless::AsyncState::WORKING;
  }

  bool badRequest(Error error) {
    isSendingSyncCommand = false;
    lastError = error;
    return false;
  }

  bool abort(Error error) {
    reset();
    lastError = error;
    return false;
  }

  bool reset() {
    bool wasEnabled = isEnabled;

    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    resetState();
    stop();
    bool success = start();

    LINK_BARRIER;
    isEnabled = wasEnabled;
    LINK_BARRIER;

    return success;
  }

  void resetState() {
    LINK_BARRIER;
    linkRawWireless._resetState();

    sessionState.recvFlag = false;
    sessionState.recvTimeout = 0;
    sessionState.signalLevelCalled = false;
    sessionState.sendReceiveLatch = false;
    sessionState.shouldWaitForServer = false;
    sessionState.didReceiveFirstPacketFromServer = false;
    sessionState.inflightCount = 0;
    sessionState.forwardedCount = 0;
    sessionState.lastPacketId = 0;
    sessionState.lastPacketIdFromServer = 0;
    sessionState.lastAckFromServer = 0;
    sessionState.localHeartbeat = -1;
    sessionState.isResetTimeoutPending = false;
    for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++) {
      sessionState.msgTimeouts[i] = 0;
      sessionState.msgFlags[i] = false;
      sessionState.lastPacketIdFromClients[i] = 0;
      sessionState.lastAckFromClients[i] = NO_ACK_RECEIVED_YET;
      sessionState.lastHeartbeatFromClients[i] = -1;
    }
    nextAsyncCommandDataSize = 0;

    sessionState.incomingMessages.syncClear();
    sessionState.outgoingMessages.clear();

    sessionState.newIncomingMessages.clear();
    sessionState.newOutgoingMessages.syncClear();

    sessionState.newIncomingMessages.overflow = false;
    sessionState.signalLevel = SignalLevel{};

    isSendingSyncCommand = false;
    LINK_BARRIER;
  }

  void stop() {
    stopTimer();
    linkRawWireless.deactivate();
  }

  bool start() {
    startTimer();

    if (!linkRawWireless.activate(false))
      return false;

    if (!setup())
      return false;

    return true;
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

  bool setup(u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS) {
    return linkRawWireless.setup(maxPlayers);
  }

  template <typename F>
  void waitVBlanks(u32 vBlanks, F onVBlank) {
    u32 count = 0;
    u32 vCount = Link::_REG_VCOUNT;

    while (count < vBlanks) {
      if (Link::_REG_VCOUNT != vCount) {
        vCount = Link::_REG_VCOUNT;

        if (vCount == 160) {
          onVBlank();
          count++;
        }
      }
    };
  }

#ifdef LINK_WIRELESS_PROFILING_ENABLED
  void profileStart() {
    Link::_REG_TM1CNT_L = 0;
    Link::_REG_TM2CNT_L = 0;

    Link::_REG_TM1CNT_H = 0;
    Link::_REG_TM2CNT_H = 0;

    Link::_REG_TM2CNT_H = Link::_TM_ENABLE | Link::_TM_CASCADE;
    Link::_REG_TM1CNT_H = Link::_TM_ENABLE | Link::_TM_FREQ_1;
  }

  u32 profileStop() {
    Link::_REG_TM1CNT_H = 0;
    Link::_REG_TM2CNT_H = 0;

    return (Link::_REG_TM1CNT_L | (Link::_REG_TM2CNT_L << 16));
  }
#endif
};

extern LinkWireless* linkWireless;

/**
 * @brief VBLANK interrupt handler.
 */
inline void LINK_WIRELESS_ISR_VBLANK() {
  linkWireless->_onVBlank();
}

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_WIRELESS_ISR_SERIAL() {
  linkWireless->_onSerial();
}

/**
 * @brief TIMER interrupt handler.
 */
inline void LINK_WIRELESS_ISR_TIMER() {
  linkWireless->_onTimer();
}

/**
 * NOTES:
 * When using `LINK_WIRELESS_ENABLE_NESTED_IRQ`:
 *   - Any user ISR can interrupt the library ISRs.
 *     * The SERIAL ISR only enables nested interrupts after completing the
 *       acknowledge with the Wireless Adapter.
 *   - SERIAL ISR can interrupt TIMER ISR.
 *     -> This doesn't cause data races since TIMER ISR only works when
 *        there is no active async task.
 *     -> When TIMER ISR starts an async task (`transferAsync(...)`),
 *        nested interrupts are disabled (`REG_IME = 0`) and SERIAL cannot
 *        interrupt anymore.
 *   - TIMER interrupts are skipped if SERIAL ISR is running.
 *   - VBLANK interrupts are postponed if SERIAL or TIMER ISRs are running.
 *   - Nobody can interrupt VBLANK ISR.
 */

#endif  // LINK_WIRELESS_H
