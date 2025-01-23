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
//       linkWireless->getServers(servers);
//       if (servers[0].id == LINK_WIRELESS_END) return;
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
//       linkWireless->receive(messages);
//       if (messages[0].packetId != LINK_WIRELESS_END) {
//         // ...
//       }
// - 8) Disconnect:
//       linkWireless->activate();
//       // (resets the adapter)
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// `send(...)` restrictions:
// - 0xFFFF is a reserved value, so don't send it!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include "LinkRawWireless.hpp"

#ifndef LINK_WIRELESS_QUEUE_SIZE
/**
 * @brief Buffer size (how many incoming and outgoing messages the queues can
 * store at max). The default value is `30`, which seems fine for most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around `960` bytes. There's a double-buffered incoming queue and a
 * double-buffered outgoing queue (to avoid data races).
 * \warning You can approximate the usage with `LINK_WIRELESS_QUEUE_SIZE * 32`.
 */
#define LINK_WIRELESS_QUEUE_SIZE 30
#endif

#ifndef LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
/**
 * @brief Max server transfer length per timer tick. Must be in the range
 * `[6;20]`. The default value is `20`, but you might want to set it a bit lower
 * to reduce CPU usage.
 */
#define LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH 20
#endif

#ifndef LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH
/**
 * @brief Max client transfer length per timer tick. Must be in the range
 * `[2;4]`. The default value is `4`. Changing this is not recommended, it's
 * already too low.
 */
#define LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#endif

#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM
/**
 * @brief Put Interrupt Service Routines (ISR) in IWRAM (uncomment to enable).
 * This can significantly improve performance due to its faster access, but it's
 * disabled by default to conserve IWRAM space, which is limited.
 * \warning If you enable this, make sure that `LinkWireless.cpp` gets compiled!
 * For example, in a Makefile-based project, verify that the file is in your
 * `SRCDIRS` list.
 */
// #define LINK_WIRELESS_PUT_ISR_IN_IWRAM
#endif

#ifndef LINK_WIRELESS_ENABLE_NESTED_IRQ
/**
 * @brief Allow LINK_WIRELESS_ISR_* functions to be interrupted (uncomment to
 * enable).
 * This can be useful, for example, if your audio engine requires calling a
 * VBlank handler with precise timing.
 * \warning This won't produce any effect if `LINK_WIRELESS_PUT_ISR_IN_IWRAM` is
 * disabled.
 */
// #define LINK_WIRELESS_ENABLE_NESTED_IRQ
#endif

#ifndef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
/**
 * @brief Use send/receive latch (uncomment to enable).
 * This makes it alternate between sends and receives on each timer tick
 * (instead of doing both things). Enabling it will introduce some latency but
 * also reduce overall CPU usage.
 */
// #define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
#endif

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
/**
 * @brief Optimize the library for two players (uncomment to enable).
 * This will make the code smaller and use less CPU. It will also let you
 * "misuse" 5 bits from the packet header to send small packets really fast
 * (e.g. pressed keys) without confirmation, using the `QUICK_SEND` and
 * `QUICK_RECEIVE` properties.
 */
// #define LINK_WIRELESS_TWO_PLAYERS_ONLY
#endif

LINK_VERSION_TAG LINK_WIRELESS_VERSION = "vLinkWireless/v8.0.0";

#define LINK_WIRELESS_MAX_PLAYERS LINK_RAW_WIRELESS_MAX_PLAYERS
#define LINK_WIRELESS_MIN_PLAYERS 2
#define LINK_WIRELESS_END 0
#define LINK_WIRELESS_MAX_SERVERS LINK_RAW_WIRELESS_MAX_SERVERS
#define LINK_WIRELESS_MAX_GAME_ID 0x7FFF
#define LINK_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_WIRELESS_DEFAULT_TIMEOUT 10
#define LINK_WIRELESS_DEFAULT_INTERVAL 50
#define LINK_WIRELESS_DEFAULT_SEND_TIMER_ID 3

#define LINK_WIRELESS_RESET_IF_NEEDED                   \
  if (!isEnabled)                                       \
    return false;                                       \
  if (linkRawWireless.getState() == State::NEEDS_RESET) \
    if (!reset())                                       \
      return false;

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
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
  static constexpr int PACKET_ID_BITS = 5;
#else
  static constexpr int PACKET_ID_BITS = 6;
#endif
  static constexpr int MAX_PACKET_IDS = 1 << PACKET_ID_BITS;
  static constexpr int PACKET_ID_MASK = MAX_PACKET_IDS - 1;
  static constexpr int MSG_PING = 0xFFFF;
  static constexpr int BROADCAST_SEARCH_WAIT_FRAMES = 60;
  static constexpr int MAX_COMMAND_TRANSFER_LENGTH = 22;

 public:
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
  u32 QUICK_SEND = 0;
  u32 QUICK_RECEIVE = 0;
#endif

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

  enum Error {
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
    u32 packetId = 0;

    u16 data;
    u8 playerId = 0;
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
   * @param maxPlayers Maximum number of allowed players. If your game only
   * supports -for example- two players, set this to `2` as it will make
   * transfers faster.
   * @param timeout Number of *frames* without receiving *any* data to reset the
   * connection.
   * @param interval Number of *1024-cycle ticks* (61.04μs) between transfers
   * *(50 = 3.052ms)*. It's the interval of Timer #`sendTimerId`. Lower values
   * will transfer faster but also consume more CPU.
   * @param sendTimerId GBA Timer to use for sending.
   * \warning You can use `Link::perFrame(...)` to convert from *packets per
   * frame* to *interval values*.
   */
  explicit LinkWireless(bool forwarding = true,
                        bool retransmission = true,
                        u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS,
                        u32 timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
                        u16 interval = LINK_WIRELESS_DEFAULT_INTERVAL,
                        u8 sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID) {
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    maxPlayers = 2;
#endif

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

    lastError = NONE;
    isEnabled = false;

    LINK_BARRIER;
    bool success = reset();
    LINK_BARRIER;

    isEnabled = true;
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
    isEnabled = false;

    resetState();
    stopTimer();
    startTimer();

    if (!linkRawWireless.restoreExistingConnection() ||
        linkRawWireless.sessionState.playerCount > config.maxPlayers) {
      deactivate();
      return false;
    }

    isEnabled = true;
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

    lastError = NONE;
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
      return badRequest(WRONG_STATE);
    if (Link::strlen(gameName) > LINK_WIRELESS_MAX_GAME_NAME_LENGTH)
      return badRequest(GAME_NAME_TOO_LONG);
    if (Link::strlen(userName) > LINK_WIRELESS_MAX_USER_NAME_LENGTH)
      return badRequest(USER_NAME_TOO_LONG);

    isSendingSyncCommand = true;
    if (isAsyncCommandActive())
      return badRequest(BUSY_TRY_AGAIN);

    if (linkRawWireless.getState() != State::SERVING) {
      if (!setup(config.maxPlayers))
        return abort(COMMAND_FAILED);
    }

    bool success = linkRawWireless.broadcast(gameName, userName, gameId, false);

    if (linkRawWireless.getState() != State::SERVING)
      success = success && linkRawWireless.startHost(false);

    if (!success)
      return abort(COMMAND_FAILED);

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
      return badRequest(WRONG_STATE);

    isSendingSyncCommand = true;
    if (isAsyncCommandActive())
      return badRequest(BUSY_TRY_AGAIN);

    LinkRawWireless::PollConnectionsResponse response;
    bool success = linkRawWireless.endHost(response);

    if (!success)
      return abort(COMMAND_FAILED);

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
   * \warning On clients, this action can fail if the adapter is busy. In that
   * case, this will return `false` and `getLastError()` will be
   * `BUSY_TRY_AGAIN`.
   */
  bool getSignalLevel(SignalLevelResponse& response) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive())
      return badRequest(WRONG_STATE);

    if (linkRawWireless.getState() == LinkRawWireless::State::SERVING) {
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++)
        response.signalLevels[i] = sessionState.signalLevel.level[i];
      return true;
    }

    isSendingSyncCommand = true;
    if (isAsyncCommandActive())
      return badRequest(BUSY_TRY_AGAIN);

    bool success = linkRawWireless.getSignalLevel(response);

    if (!success)
      return abort(COMMAND_FAILED);

    LINK_BARRIER;
    isSendingSyncCommand = false;
    LINK_BARRIER;

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers.
   * @param servers The array to be filled with data.
   * \warning This action takes 1 second to complete.
   * \warning For an async version, see `getServersAsyncStart()`.
   */
  bool getServers(Server servers[]) {
    return getServers(servers, []() {});
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers.
   * @param servers The array to be filled with data.
   * @param onWait A function which will be invoked each time VBlank starts.
   * \warning This action takes 1 second to complete.
   * \warning For an async version, see `getServersAsyncStart()`.
   */
  template <typename F>
  bool getServers(Server servers[], F onWait) {
    if (!getServersAsyncStart())
      return false;

    waitVBlanks(BROADCAST_SEARCH_WAIT_FRAMES, onWait);

    if (!getServersAsyncEnd(servers))
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
      return badRequest(WRONG_STATE);

    bool success = linkRawWireless.broadcastReadStart();

    if (!success)
      return abort(COMMAND_FAILED);

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers. Changes the state to `AUTHENTICATED` again.
   * @param servers The array to be filled with data.
   */
  bool getServersAsyncEnd(Server servers[]) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless.getState() != State::SEARCHING)
      return badRequest(WRONG_STATE);

    LinkRawWireless::BroadcastReadPollResponse response;
    bool success1 = linkRawWireless.broadcastReadPoll(response);

    if (!success1)
      return abort(COMMAND_FAILED);

    bool success2 = linkRawWireless.broadcastReadEnd();

    if (!success2)
      return abort(COMMAND_FAILED);

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
      return badRequest(WRONG_STATE);

    bool success = linkRawWireless.connect(serverId);

    if (!success)
      return abort(COMMAND_FAILED);

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
      return badRequest(WRONG_STATE);

    LinkRawWireless::ConnectionStatus response;
    bool success1 = linkRawWireless.keepConnecting(response);

    if (!success1)
      return abort(COMMAND_FAILED);

    if (response.phase == LinkRawWireless::ConnectionPhase::STILL_CONNECTING)
      return true;
    else if (response.phase == LinkRawWireless::ConnectionPhase::ERROR)
      return abort(COMMAND_FAILED);

    auto success2 = linkRawWireless.finishConnection();
    if (!success2)
      return abort(COMMAND_FAILED);

    return true;
  }

  /**
   * @brief Enqueues `data` to be sent to other nodes.
   * @param data The value to be sent.
   */
  bool send(u16 data, int _author = -1) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive())
      return badRequest(WRONG_STATE);

    if (!canAddNewMessage()) {
      if (_author < 0)
        lastError = BUFFER_IS_FULL;
      return false;
    }

    Message message;
    message.playerId =
        _author >= 0 ? _author : linkRawWireless.sessionState.currentPlayerId;
    message.data = data;

    sessionState.newOutgoingMessages.syncPush(message);

    return true;
  }

  /**
   * @brief Fills the `messages` array with incoming messages, forwarding if
   * needed.
   * @param messages The array to be filled with data.
   */
  bool receive(Message messages[]) {
    if (!isSessionActive())
      return false;

    LINK_BARRIER;
    sessionState.incomingMessages.startReading();
    LINK_BARRIER;

    u32 i = 0;
    while (!sessionState.incomingMessages.isEmpty()) {
      auto message = sessionState.incomingMessages.pop();
      messages[i] = message;
#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
      forwardMessageIfNeeded(message);
#endif
      i++;
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
   * @brief Returns whether the internal receive queue lost messages at some
   * point due to being full. This can happen if your queue size is too low, if
   * you receive too much data without calling `receive(...)` enough times, or
   * if excessive `receive(...)` calls prevent the ISR from copying data. After
   * this call, the overflow flag is cleared if `clear` is `true` (default
   * behavior).
   */
  [[nodiscard]] bool didQueueOverflow(bool clear = true) {
    bool overflow = sessionState.newIncomingMessages.overflow;
    if (clear)
      sessionState.newIncomingMessages.overflow = false;
    return overflow;
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
      lastError = NONE;
    return error;
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
   * @brief Returns the number of pending outgoing messages.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _getPendingCount() {
    return sessionState.outgoingMessages.size();
  }

  /**
   * @brief Returns the last packet ID.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketId() { return sessionState.lastPacketId; }

  /**
   * @brief Returns the last confirmation received from player ID 1.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastConfirmationFromClient1() {
    return sessionState.lastConfirmationFromClients[1];
  }

  /**
   * @brief Returns the last packet ID received from player ID 1.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketIdFromClient1() {
    return sessionState.lastPacketIdFromClients[1];
  }

  /**
   * @brief Returns the last confirmation received from the server.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastConfirmationFromServer() {
    return sessionState.lastConfirmationFromServer;
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

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    if (interrupt) {
      pendingVBlank = true;
      return;
    }
#endif
#endif

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    profileStart();
#endif

    if (!isSessionActive())
      return;

    if (isConnected() && !sessionState.recvFlag)
      sessionState.recvTimeout++;
    if (sessionState.recvTimeout >= config.timeout)
      return (void)abort(TIMEOUT);

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
    trackRemoteTimeouts();
    if (!checkRemoteTimeouts())
      return (void)abort(REMOTE_TIMEOUT);
#endif

    sessionState.recvFlag = false;
    sessionState.signalLevelCalled = false;
    sessionState.pingSent = false;

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    vblankTime += profileStop();
    vblankIRQs++;
#endif
  }

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
  void _onSerial();
  void _onTimer();
#else
  void _onSerial() { __onSerial(); }
  void _onTimer() { __onTimer(); }
#endif

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  LINK_INLINE void __onSerial() {
    if (!isEnabled)
      return;

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    profileStart();
#endif

    int status = linkRawWireless._onSerial(false);
    if (status <= -4) {
      return (void)abort(ACKNOWLEDGE_FAILED);
    } else if (status > 0) {
      auto result = linkRawWireless._getAsyncCommandResultRef();
      processAsyncCommand(result);
    }

#ifdef LINK_WIRELESS_PROFILING_ENABLED
    serialTime += profileStop();
    serialIRQs++;
#endif
  }

  /**
   * @brief This method is called by the TIMER interrupt handler.
   * \warning This is internal API!
   */
  LINK_INLINE void __onTimer() {
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

  struct Config {
    bool forwarding;
    bool retransmission;
    u8 maxPlayers;
    u32 timeout;   // can be changed in realtime
    u16 interval;  // can be changed in realtime, but call `resetTimer()`
    u8 sendTimerId;
  };

  /**
   * @brief LinkWireless configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
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
    bool pingSent = false;
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
    bool sendReceiveLatch = false;  // true = send ; false = receive
    bool shouldWaitForServer = false;
#endif

    bool didReceiveLastPacketIdFromServer = false;
    u32 lastPacketId = 0;
    u32 lastPacketIdFromServer = 0;
    u32 lastConfirmationFromServer = 0;
    u32 lastPacketIdFromClients[LINK_WIRELESS_MAX_PLAYERS];
    u32 lastConfirmationFromClients[LINK_WIRELESS_MAX_PLAYERS];
  };

  struct MessageHeader {
    unsigned int partialPacketId : PACKET_ID_BITS;
    unsigned int isConfirmation : 1;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    unsigned int playerId : 1;
    unsigned int quickData : 5;
#else
    unsigned int playerId : 3;
    unsigned int clientCount : 2;
#endif
    unsigned int dataChecksum : 4;
  };

  union MessageHeaderSerializer {
    MessageHeader asStruct;
    u16 asInt;
  };

  using CommandResult = LinkRawWireless::CommandResult;

  LinkRawWireless linkRawWireless;
  SessionState sessionState;
  u32 nextAsyncCommandData[MAX_COMMAND_TRANSFER_LENGTH];
  u32 nextAsyncCommandDataSize = 0;
  volatile bool isSendingSyncCommand = false;
  volatile Error lastError = NONE;
  volatile bool isEnabled = false;

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  volatile bool interrupt = false, pendingVBlank = false;
#endif
#endif

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
  void forwardMessageIfNeeded(Message& message) {
    if (linkRawWireless.getState() == State::SERVING && config.forwarding &&
        linkRawWireless.sessionState.playerCount > 2)
      send(message.data, message.playerId);
  }
#endif

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  void irqEnd() {
    Link::_REG_IME = 0;
    interrupt = false;
    LINK_BARRIER;
    if (pendingVBlank) {
      _onVBlank();
      pendingVBlank = false;
    }
  }
#endif
#endif

  bool canAddNewMessage() { return !sessionState.newOutgoingMessages.isFull(); }

  LINK_INLINE void processAsyncCommand(
      const LinkRawWireless::CommandResult* commandResult) {  // (irq only)
    if (!commandResult->success) {
      return (void)abort(
          commandResult->commandId == LinkRawWireless::COMMAND_SEND_DATA
              ? SEND_DATA_FAILED
          : commandResult->commandId == LinkRawWireless::COMMAND_RECEIVE_DATA
              ? RECEIVE_DATA_FAILED
              : COMMAND_FAILED);
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
          linkRawWireless.sessionState.playerCount =
              Link::_min(players, config.maxPlayers);
        }

        break;
      }
      case LinkRawWireless::COMMAND_SEND_DATA: {
        // SendData (end)

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        if (linkRawWireless.getState() == State::CONNECTED)
          sessionState.shouldWaitForServer = true;
        sessionState.sendReceiveLatch = !sessionState.sendReceiveLatch;
#else
        if (linkRawWireless.getState() == State::SERVING) {
          // ReceiveData (start)
          sendCommandAsync(LinkRawWireless::COMMAND_RECEIVE_DATA);
        }
#endif

        break;
      }
      case LinkRawWireless::COMMAND_RECEIVE_DATA: {
        // ReceiveData (end)

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        sessionState.sendReceiveLatch =
            sessionState.shouldWaitForServer || !sessionState.sendReceiveLatch;
#endif

        if (commandResult->dataSize == 0)
          break;

        sessionState.recvFlag = true;
        sessionState.recvTimeout = 0;

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        sessionState.shouldWaitForServer = false;
#endif

        addIncomingMessagesFromData(commandResult);

#ifndef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        if (linkRawWireless.getState() == State::CONNECTED) {
          // SendData (start)
          sendPendingData();
        }
#endif

        break;
      }
      default: {
      }
    }
  }

  LINK_INLINE void checkConnectionsOrTransferData() {  // (irq only)
    if (linkRawWireless.getState() == State::SERVING &&
        !sessionState.signalLevelCalled) {
      // SignalLevel (start)
      if (sendCommandAsync(LinkRawWireless::COMMAND_SIGNAL_LEVEL))
        sessionState.signalLevelCalled = true;
    } else if (linkRawWireless.getState() == State::CONNECTED ||
               isConnected()) {
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
      bool shouldReceive =
          !sessionState.sendReceiveLatch || sessionState.shouldWaitForServer;
#else
      bool shouldReceive = linkRawWireless.getState() == State::CONNECTED;
#endif

      if (shouldReceive) {
        // ReceiveData (start)
        sendCommandAsync(LinkRawWireless::COMMAND_RECEIVE_DATA);
      } else {
        // SendData (start)
        sendPendingData();
      }
    }
  }

  void sendPendingData() {  // (irq only)
    copyOutgoingState();
    int lastPacketId = setDataFromOutgoingMessages();
    if (sendCommandAsync(LinkRawWireless::COMMAND_SEND_DATA, true))
      clearOutgoingMessagesIfNeeded(lastPacketId);
  }

  int setDataFromOutgoingMessages() {  // (irq only)
    u32 maxTransferLength = getDeviceTransferLength();

    addAsyncData(0, true);

    if (config.retransmission)
      addConfirmations();
    else
      addPingMessageIfNeeded();

    int lastPacketId = -1;

    sessionState.outgoingMessages.forEach(
        [this, maxTransferLength, &lastPacketId](Message message) {
          u16 header = buildMessageHeader(message.playerId, message.packetId,
                                          buildChecksum(message.data));
          u32 rawMessage = Link::buildU32(header, message.data);

          addAsyncData(rawMessage);
          lastPacketId = message.packetId;

          if (nextAsyncCommandDataSize > maxTransferLength)
            return false;

          return true;
        });

    // (add wireless header)
    u32 bytes = (nextAsyncCommandDataSize - 1) * 4;
    nextAsyncCommandData[0] = linkRawWireless.getSendDataHeaderFor(bytes);

    return lastPacketId;
  }

  void addIncomingMessagesFromData(const CommandResult* result) {  // (irq only)
    for (u32 i = 1; i < result->dataSize; i++) {
      u32 rawMessage = result->data[i];
      u16 headerInt = Link::msB32(rawMessage);
      u16 data = Link::lsB32(rawMessage);

      MessageHeaderSerializer serializer;
      serializer.asInt = headerInt;

      MessageHeader header = serializer.asStruct;
      u32 partialPacketId = header.partialPacketId;
      bool isConfirmation = header.isConfirmation;
      u8 remotePlayerId = Link::_min(header.playerId, config.maxPlayers - 1);
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
      QUICK_RECEIVE = header.quickData;
      u8 remotePlayerCount = 2;
#else
      u8 remotePlayerCount = LINK_WIRELESS_MIN_PLAYERS + header.clientCount;
#endif
      u32 checksum = header.dataChecksum;
      bool isPing = data == MSG_PING;

      if (checksum != buildChecksum(data))
        continue;

      Message message;
      message.packetId = partialPacketId;
      message.data = data;
      message.playerId = remotePlayerId;
      if (!acceptMessage(message, isConfirmation, remotePlayerCount))
        continue;
      if (config.retransmission && isConfirmation &&
          !handleConfirmation(message))
        continue;

      sessionState.msgTimeouts[0] = 0;
      sessionState.msgTimeouts[remotePlayerId] = 0;
      sessionState.msgFlags[0] = true;
      sessionState.msgFlags[remotePlayerId] = true;

      if (!isPing && !isConfirmation)
        sessionState.newIncomingMessages.push(message);
    }
    copyIncomingState();
  }

  bool acceptMessage(Message& message,
                     bool isConfirmation,
                     u32 remotePlayerCount) {  // (irq only)
    if (linkRawWireless.getState() == State::SERVING) {
      u32 expectedPacketId =
          (sessionState.lastPacketIdFromClients[message.playerId] + 1) %
          MAX_PACKET_IDS;
      // if message.packetId > expectedPacketId = packet loss (gap)
      // if message.packetId < expectedPacketId = retransmission of old packet

      if (config.retransmission && !isConfirmation &&
          message.packetId != expectedPacketId)
        return false;

      if (!isConfirmation)
        message.packetId =
            ++sessionState.lastPacketIdFromClients[message.playerId];
    } else {
      u32 expectedPacketId =
          (sessionState.lastPacketIdFromServer + 1) % MAX_PACKET_IDS;

      if (config.retransmission && !isConfirmation &&
          message.packetId != expectedPacketId)
        return false;

      linkRawWireless.sessionState.playerCount = remotePlayerCount;

      if (!isConfirmation)
        message.packetId = ++sessionState.lastPacketIdFromServer;
    }

    bool isMessageFromCurrentPlayer =
        !isConfirmation &&
        message.playerId == linkRawWireless.sessionState.currentPlayerId;

    return !isMessageFromCurrentPlayer;
  }

  void clearOutgoingMessagesIfNeeded(int lastPacketId) {  // (irq only)
    if (!config.retransmission && lastPacketId > -1)
      removeConfirmedMessages(lastPacketId);
  }

  void addPingMessageIfNeeded() {  // (irq only)
    if (sessionState.outgoingMessages.isEmpty() && !sessionState.pingSent) {
      Message pingMessage;
      pingMessage.packetId = newPacketId();
      pingMessage.playerId = linkRawWireless.sessionState.currentPlayerId;
      pingMessage.data = MSG_PING;
      sessionState.outgoingMessages.push(pingMessage);
      sessionState.pingSent = true;
    }
  }

  void addConfirmations() {  // (irq only)
    if (linkRawWireless.getState() == State::SERVING) {
#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
      if (config.maxPlayers > 2 &&
          (sessionState.lastPacketIdFromClients[1] == 0 ||
           sessionState.lastPacketIdFromClients[2] == 0 ||
           sessionState.lastPacketIdFromClients[3] == 0 ||
           sessionState.lastPacketIdFromClients[4] == 0)) {
        u32 lastPacketId = sessionState.lastPacketId;
        u16 header = buildConfirmationHeader(0, lastPacketId);
        u32 rawMessage = Link::buildU32(header, lastPacketId & 0xFFFF);
        addAsyncData(rawMessage);
      }
#endif

      for (int i = 0; i < linkRawWireless.sessionState.playerCount - 1; i++) {
        u32 confirmationData = sessionState.lastPacketIdFromClients[1 + i];
        u16 header = buildConfirmationHeader(1 + i, confirmationData);
        u32 rawMessage = Link::buildU32(header, confirmationData & 0xFFFF);
        addAsyncData(rawMessage);
      }
    } else {
      u32 confirmationData = sessionState.lastPacketIdFromServer;
      u16 header = buildConfirmationHeader(
          linkRawWireless.sessionState.currentPlayerId, confirmationData);
      u32 rawMessage = Link::buildU32(header, confirmationData & 0xFFFF);
      addAsyncData(rawMessage);
    }
  }

  bool handleConfirmation(Message confirmation) {  // (irq only)
    u32 confirmationData = (confirmation.packetId << 16) | confirmation.data;

    if (linkRawWireless.getState() == State::CONNECTED) {
      if (confirmation.playerId == 0 &&
          !sessionState.didReceiveLastPacketIdFromServer) {
        sessionState.lastPacketIdFromServer = confirmationData;
        sessionState.didReceiveLastPacketIdFromServer = true;
      } else if (confirmation.playerId ==
                 linkRawWireless.sessionState.currentPlayerId) {
        handleServerConfirmation(confirmationData);
      } else {
        return false;
      }
    } else {
      handleClientConfirmation(confirmationData, confirmation.playerId);
    }

    return true;
  }

  void handleServerConfirmation(u32 confirmationData) {  // (irq only)
    sessionState.lastConfirmationFromServer = confirmationData;
    removeConfirmedMessages(confirmationData);
  }

  void handleClientConfirmation(u32 confirmationData,
                                u8 playerId) {  // (irq only)
    sessionState.lastConfirmationFromClients[playerId] = confirmationData;

    u32 min = 0xFFFFFFFF;
    for (int i = 0; i < config.maxPlayers - 1; i++) {
      u32 _confirmationData = sessionState.lastConfirmationFromClients[1 + i];
      if (_confirmationData > 0 && _confirmationData < min)
        min = _confirmationData;
    }
    if (min < 0xFFFFFFFF)
      removeConfirmedMessages(min);
  }

  void removeConfirmedMessages(u32 confirmationData) {  // (irq only)
    while (!sessionState.outgoingMessages.isEmpty() &&
           sessionState.outgoingMessages.peek().packetId <= confirmationData)
      sessionState.outgoingMessages.pop();
  }

  u16 buildConfirmationHeader(u8 playerId,
                              u32 confirmationData) {  // (irq only)
    // confirmation messages "repurpose" some message header fields:
    //     packetId => high 6 bits of confirmation
    //     data     => low 16 bits of confirmation
    u8 highPart = (confirmationData >> 16) & PACKET_ID_MASK;
    u16 lowPart = confirmationData & 0xFFFF;
    return buildMessageHeader(playerId, highPart, buildChecksum(lowPart), true);
  }

  u16 buildMessageHeader(u8 playerId,
                         u32 packetId,
                         u8 dataChecksum,
                         bool isConfirmation = false) {  // (irq only)
    MessageHeader header;
    header.partialPacketId = packetId % MAX_PACKET_IDS;
    header.isConfirmation = isConfirmation;
    header.playerId = playerId;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    header.quickData = QUICK_SEND;
#else
    header.clientCount =
        linkRawWireless.sessionState.playerCount - LINK_WIRELESS_MIN_PLAYERS;
#endif
    header.dataChecksum = dataChecksum;

    MessageHeaderSerializer serializer;
    serializer.asStruct = header;
    return serializer.asInt;
  }

  u32 buildChecksum(u16 data) {  // (irq only)
    // (hamming weight)
    return __builtin_popcount(data) % 16;
  }

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
  void trackRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < linkRawWireless.sessionState.playerCount; i++) {
      if (i != linkRawWireless.sessionState.currentPlayerId &&
          !sessionState.msgFlags[i])
        sessionState.msgTimeouts[i]++;
      sessionState.msgFlags[i] = false;
    }
  }

  bool checkRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < linkRawWireless.sessionState.playerCount; i++) {
      if ((i == 0 || linkRawWireless.getState() == State::SERVING) &&
          sessionState.msgTimeouts[i] > config.timeout)
        return false;
    }

    return true;
  }
#endif

  u32 getDeviceTransferLength() {  // (irq only)
    return linkRawWireless.getState() == State::SERVING
               ? LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
               : LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH;
  }

  void copyOutgoingState() {  // (irq only)
    if (sessionState.newOutgoingMessages.isWriting())
      return;

    while (!sessionState.newOutgoingMessages.isEmpty() &&
           !sessionState.outgoingMessages.isFull()) {
      auto message = sessionState.newOutgoingMessages.pop();
      message.packetId = newPacketId();
      sessionState.outgoingMessages.push(message);
    }
  }

  void copyIncomingState() {  // (irq only)
    if (sessionState.incomingMessages.isReading())
      return;

    while (!sessionState.newIncomingMessages.isEmpty() &&
           !sessionState.incomingMessages.isFull()) {
      auto message = sessionState.newIncomingMessages.pop();
      sessionState.incomingMessages.push(message);
    }
  }

  u32 newPacketId() {  // (irq only)
    return ++sessionState.lastPacketId;
  }

  bool sendCommandAsync(u8 type, bool withData = false) {  // (irq only)
    if (isSendingSyncCommand)
      return false;

    u32 size = withData ? nextAsyncCommandDataSize : 0;
    return linkRawWireless.sendCommandAsync(type, nextAsyncCommandData, size,
                                            false, true);
  }

  void addAsyncData(u32 value, bool start = false) {  // (irq only)
    if (start)
      nextAsyncCommandDataSize = 0;
    nextAsyncCommandData[nextAsyncCommandDataSize] = value;
    nextAsyncCommandDataSize++;
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
    linkRawWireless._resetState();

#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    QUICK_SEND = 0;
    QUICK_RECEIVE = 0;
#endif
    sessionState.recvFlag = false;
    sessionState.recvTimeout = 0;
    sessionState.signalLevelCalled = false;
    sessionState.pingSent = false;
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
    sessionState.sendReceiveLatch = false;
    sessionState.shouldWaitForServer = false;
#endif
    sessionState.didReceiveLastPacketIdFromServer = false;
    sessionState.lastPacketId = 0;
    sessionState.lastPacketIdFromServer = 0;
    sessionState.lastConfirmationFromServer = 0;
    for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++) {
      sessionState.msgTimeouts[i] = 0;
      sessionState.msgFlags[i] = false;
      sessionState.lastPacketIdFromClients[i] = 0;
      sessionState.lastConfirmationFromClients[i] = 0;
    }
    nextAsyncCommandDataSize = 0;

    sessionState.incomingMessages.syncClear();
    sessionState.outgoingMessages.clear();

    sessionState.newIncomingMessages.clear();
    sessionState.newOutgoingMessages.syncClear();

    sessionState.newIncomingMessages.overflow = false;
    sessionState.signalLevel = SignalLevel{};

    isSendingSyncCommand = false;
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

#endif  // LINK_WIRELESS_H
